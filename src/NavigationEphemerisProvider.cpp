#include "NavigationEphemerisProvider.h"

#include "celestial_navigation_pi.h"

#include "eclipse/data_pack.h"
#include "eclipse/dut1.h"
#include "eclipse/navigation.h"
#include "eclipse/spk.h"
#include "eclipse/time.h"

#include <algorithm>
#include <cmath>
#include <ctime>

#include <wx/filename.h>
#include <wx/utils.h>

namespace celestial_navigation {
namespace {
constexpr double kAuKm = 149597870.7;

int TargetId(const wxString& body) {
  if (body == "Sun") return 10;
  if (body == "Moon") return 301;
  if (body == "Mercury") return 199;
  if (body == "Venus") return 299;
  return 0;  // Stars and planets without compact-kernel body centres.
}

wxString KernelPath() {
#ifdef UNIT_TESTS
  // Baseline tests deliberately remain analytical. Dedicated integration
  // tests opt into the same DE440s path without requiring wxStandardPaths.
  wxString enabled;
  if (!wxGetEnv("CELNAV_TEST_DE440_ENABLE", &enabled) || enabled != "1")
    return wxString();
  wxString test_path;
  if (wxGetEnv("CELNAV_TEST_DE440_PATH", &test_path)) return test_path;
  return wxString::FromUTF8(ECLIPSE_DE440_TEST_PATH);
#else
  return celestial_navigation_pi::StandardPath() + "eclipse" +
         wxFileName::GetPathSeparator() + "de440s.bsp";
#endif
}

struct ThreadKernel {
  eclipse::SpkKernel kernel;
  wxString path;
  time_t modified = 0;
  bool attempted = false;
  std::string error;
};

eclipse::SpkKernel* VerifiedKernel(std::string* reason) {
  static thread_local ThreadKernel cache;
  const wxString path = KernelPath();
  if (path.empty()) {
    if (reason) *reason = "DE440s is not enabled for this test";
    return nullptr;
  }
  const wxFileName file(path);
  if (!file.FileExists()) {
    if (reason) *reason = "DE440s is not installed";
    return nullptr;
  }
  const wxDateTime mtime = file.GetModificationTime();
  const time_t modified = mtime.IsValid() ? mtime.GetTicks() : 0;
  if (!cache.attempted || cache.path != path || cache.modified != modified) {
    cache.attempted = true;
    cache.path = path;
    cache.modified = modified;
    cache.error.clear();
    const auto status = eclipse::VerifyDe440s(path.ToStdString());
    if (!status.valid)
      cache.error = status.error;
    else if (!cache.kernel.Open(path.ToStdString(), &cache.error))
      cache.error = "Unable to open verified DE440s: " + cache.error;
  }
  if (!cache.error.empty() || !cache.kernel.IsOpen()) {
    if (reason) *reason = cache.error.empty() ?
        "DE440s could not be opened" : cache.error;
    return nullptr;
  }
  return &cache.kernel;
}

bool PrepareEpoch(const wxDateTime& utc_fields,
                  double dut1_override_seconds,
                  eclipse::SpkKernel** kernel,
                  eclipse::NavigationEpoch* epoch,
                  eclipse::Dut1Result* dut1_used,
                  std::string* reason) {
  if (!utc_fields.IsValid() || !kernel || !epoch || !dut1_used) {
    if (reason) *reason = "Invalid navigation epoch input";
    return false;
  }
  *kernel = VerifiedKernel(reason);
  if (!*kernel) return false;
  eclipse::CalendarDateTime utc;
  utc.year = utc_fields.GetYear();
  utc.month = static_cast<int>(utc_fields.GetMonth()) + 1;
  utc.day = utc_fields.GetDay();
  utc.hour = utc_fields.GetHour();
  utc.minute = utc_fields.GetMinute();
  utc.second = utc_fields.GetSecond() + utc_fields.GetMillisecond() / 1000.0;
  if (utc.year < 1972) {
    if (reason) *reason =
        "Automatic UTC-to-TT conversion is unsupported before 1972";
    return false;
  }
  double utc_jd = 0.0;
  if (!eclipse::CalendarToJulianDate(utc, &utc_jd, reason)) return false;
  const auto dut1 = eclipse::LookupDut1(utc_jd);
  const bool override_dut1 = std::isfinite(dut1_override_seconds);
  const double selected_dut1 = override_dut1 ? dut1_override_seconds :
                               dut1.available ? dut1.seconds : 0.0;
  const double tai = std::isfinite(dut1.tai_minus_utc) ?
      dut1.tai_minus_utc : eclipse::TaiMinusUtcSeconds(utc);
  if (!std::isfinite(tai)) {
    if (reason) *reason = "TAI-UTC is unavailable at this UTC";
    return false;
  }
  if (!eclipse::MakeNavigationEpoch(utc, selected_dut1, tai, 0.0, 0.0,
                                    epoch, reason))
    return false;
  *dut1_used = dut1;
  dut1_used->seconds = selected_dut1;
  dut1_used->available = override_dut1 || dut1.available;
  return true;
}
}  // namespace

bool TryDe440NavigationSample(const wxString& body,
                              const wxDateTime& utc_fields,
                              De440NavigationSample* sample,
                              std::string* reason,
                              double dut1_override_seconds) {
  if (!sample || !utc_fields.IsValid()) {
    if (reason) *reason = "Invalid navigation sample input";
    return false;
  }
  const int target = TargetId(body);
  if (!target) {
    if (reason) *reason = "Target centre is not in compact DE440s";
    return false;
  }
  eclipse::SpkKernel* kernel = nullptr;
  eclipse::NavigationEpoch epoch;
  eclipse::Dut1Result dut1;
  if (!PrepareEpoch(utc_fields, dut1_override_seconds, &kernel, &epoch,
                    &dut1, reason))
    return false;
  eclipse::NavigationGeocentricState state;
  if (!eclipse::GeocentricNavigationState(*kernel, target, epoch,
                                          &state, reason))
    return false;
  De440NavigationSample result;
  result.gha_deg = state.gha_deg;
  result.declination_deg = state.declination_deg;
  result.aries_gha_deg = state.gha_aries_deg;
  result.range_km = state.distance_km;
  result.horizontal_parallax_deg = state.horizontal_parallax_deg;
  result.semidiameter_deg = state.semidiameter_deg;
  result.dut1_seconds = dut1.seconds;
  result.tai_minus_utc_seconds =
      (epoch.orientation.tt_jd - epoch.orientation.ut1_jd) * 86400.0 +
      dut1.seconds - 32.184;
  result.dut1_available = dut1.available;
  result.venus_phase_corrected = state.phase_corrected;
  if (target == 10)
    result.sun_range_au = state.distance_km / kAuKm;
  else {
    eclipse::NavigationGeocentricState sun;
    if (!eclipse::GeocentricNavigationState(*kernel, 10, epoch,
                                            &sun, reason))
      return false;
    result.sun_range_au = sun.distance_km / kAuKm;
  }
  *sample = result;
  return true;
}

bool TryDe440ObserverDirection(const wxString& body,
                              const wxDateTime& utc_fields,
                              double latitude_deg, double longitude_deg,
                              double height_m,
                              De440ObserverDirection* direction,
                              std::string* reason,
                              double dut1_override_seconds) {
  if (!direction) {
    if (reason) *reason = "Missing observer direction output";
    return false;
  }
  const int target = TargetId(body);
  if (!target) {
    if (reason) *reason = "Target centre is not in compact DE440s";
    return false;
  }
  eclipse::SpkKernel* kernel = nullptr;
  eclipse::NavigationEpoch epoch;
  eclipse::Dut1Result dut1;
  if (!PrepareEpoch(utc_fields, dut1_override_seconds, &kernel, &epoch,
                    &dut1, reason))
    return false;
  De440ObserverDirection result;
  if (!eclipse::ObserverApparentTargetDirection(
          *kernel, target, epoch.et_seconds, epoch.orientation,
          latitude_deg, longitude_deg, height_m,
          &result.airless_altitude_deg, &result.azimuth_deg,
          &result.range_km, reason, target == 299))
    return false;
  const double radius_km = target == 301 ? 1737.4 :
                           target == 10 ? 695700.0 : 0.0;
  if (radius_km > 0.0)
    result.semidiameter_deg =
        std::asin(std::min(1.0, radius_km / result.range_km)) *
        (180.0 / 3.14159265358979323846);
  *direction = result;
  return true;
}

}  // namespace celestial_navigation
