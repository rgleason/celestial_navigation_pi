#include "CompactEphemerisProvider.h"
#include "celestial_navigation_pi.h"
#include "UtcDateTime.h"
#include "eclipse/astronomy.h"
#include "eclipse/data_pack.h"
#include "eclipse/navigation.h"
#include "eclipse/spk.h"
#include "eclipse/time.h"
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/utils.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <stdexcept>

namespace celestial_navigation {
namespace {
constexpr double kRad = 3.14159265358979323846 / 180.0;
constexpr double kAuKm = 149597870.7;
wxString DataPath() {
#ifdef UNIT_TESTS
  wxString path;
  if (wxGetEnv("CELNAV_TEST_COMPACT_PATH", &path)) return path;
  return wxString::FromUTF8(CELNAV_COMPACT_TEST_DATA);
#else
  return celestial_navigation_pi_DataDir() + "/data/compact";
#endif
}
wxString LunarKernelPath() {
#ifdef CELESTIAL_LAB
  wxString path;
  wxGetEnv("CELNAV_LAB_DE440_PATH", &path);
  return path;
#elif defined(UNIT_TESTS)
  // Existing dedicated lunar DE440 tests use their source fixture. Ordinary
  // navigation retains its separate test opt-in.
  wxString path;
  if (wxGetEnv("CELNAV_TEST_DE440_PATH", &path)) return path;
  return wxString::FromUTF8(ECLIPSE_DE440_TEST_PATH);
#else
  return celestial_navigation_pi::StandardPath() + "eclipse/de440s.bsp";
#endif
}
int Target(const wxString& body) {
  return body == "Sun"       ? 10
         : body == "Moon"    ? 301
         : body == "Mercury" ? 199
         : body == "Venus"   ? 299
                             : 0;
}
eclipse::CalendarDateTime Calendar(const wxDateTime& time,
                                   bool instant = false) {
  eclipse::CalendarDateTime c;
#ifdef __OCPN__ANDROID__
  const auto fields = UtcDateTime::Fields(time);
#else
  const auto fields = time.GetTm(instant ? wxDateTime::UTC : wxDateTime::Local);
#endif
  c.year = fields.year;
  c.month = int(fields.mon) + 1;
  c.day = fields.mday;
  c.hour = fields.hour;
  c.minute = fields.min;
  c.second = fields.sec + fields.msec / 1000.0;
  return c;
}
eclipse::Dut1Result TimeData(
    const wxDateTime& time,
    const std::shared_ptr<const eclipse::Dut1Table>& update,
    bool instant = false) {
  double jd;
  std::string error;
  if (!time.IsValid() ||
      !eclipse::CalendarToJulianDate(Calendar(time, instant), &jd, &error))
    throw std::invalid_argument("Invalid navigation UTC: " + error);
  return eclipse::LookupDut1(jd, update);
}
void SetTime(lunar_distance::EphemerisSample* sample,
             const eclipse::Dut1Result& d) {
  sample->dut1_available = d.available;
  sample->dut1_seconds = d.seconds;
  sample->dut1_quality = d.quality;
  sample->dut1_from_update = d.from_update;
}
void SetGeometry(lunar_distance::EphemerisSample* s, double moon_gha,
                 double moon_dec, double moon_range, double body_gha,
                 double body_dec, double body_range, bool sun) {
  *s = {};
  s->moon_geographic_latitude_deg = moon_dec;
  s->moon_geographic_longitude_deg = std::remainder(-moon_gha, 360.0);
  s->body_geographic_latitude_deg = body_dec;
  s->body_geographic_longitude_deg = std::remainder(-body_gha, 360.0);
  s->predicted_distance_deg =
      lunar_distance::GreatCircleDistanceNm(
          {moon_dec, s->moon_geographic_longitude_deg},
          {body_dec, s->body_geographic_longitude_deg}) /
      60.0;
  s->moon_horizontal_parallax_deg = std::asin(6378.137 / moon_range) / kRad;
  s->moon_semidiameter_deg = std::asin(1737.4 / moon_range) / kRad;
  if (body_range > 6378.137)
    s->body_horizontal_parallax_deg = std::asin(6378.137 / body_range) / kRad;
  if (sun) s->body_semidiameter_deg = std::asin(695700.0 / body_range) / kRad;
}
struct LunarKernel {
  eclipse::SpkKernel kernel;
  std::mutex mutex;
};
}  // namespace

static std::atomic_bool compact_enabled{true};
void InitializeCompactEphemerisPreference() {
  bool enabled = true;
  if (auto* config = GetOCPNConfigObject())
    config->Read("/PlugIns/CelestialNavigation/UseCompactEphemeris", &enabled, true);
  compact_enabled.store(enabled);
}
bool CompactEphemerisEnabled() {
#ifdef __OCPN__ANDROID__
  return compact_enabled.load();
#else
  bool enabled = true;
  if (auto* config = GetOCPNConfigObject())
    config->Read("/PlugIns/CelestialNavigation/UseCompactEphemeris", &enabled,
                 true);
  return enabled;
#endif
}
void SetCompactEphemerisEnabled(bool enabled) {
  compact_enabled.store(enabled);
  if (auto* config = GetOCPNConfigObject()) {
    config->Write("/PlugIns/CelestialNavigation/UseCompactEphemeris", enabled);
    config->Flush();
  }
}
std::string CompactUtc(const wxDateTime& time, bool instant) {
  if (!time.IsValid()) throw std::invalid_argument("Invalid navigation UTC");
#ifdef __OCPN__ANDROID__
  return UtcDateTime::FormatInstant(time, "%Y-%m-%dT%H:%M:%S.%l").ToStdString();
#else
  return time
      .Format("%Y-%m-%dT%H:%M:%S.%l",
              instant ? wxDateTime::UTC : wxDateTime::Local)
      .ToStdString();
#endif
}
std::shared_ptr<const celnav::Engine> CompactEngine(std::string* reason) {
  static std::mutex mutex;
  static wxString retained_path;
  static wxDateTime retained_mtime;
  static std::shared_ptr<const celnav::Engine> retained;
  static std::string failure;
  const wxString path = DataPath();
  const wxFileName manifest(path + "/manifest.json");
  const wxDateTime mtime =
      manifest.FileExists() ? manifest.GetModificationTime() : wxDateTime();
  std::lock_guard<std::mutex> lock(mutex);
  if (!retained || retained_path != path || retained_mtime != mtime) {
    retained.reset();
    retained_path = path;
    retained_mtime = mtime;
    failure.clear();
    try {
      celnav::Options options;
      options.data_directory = path.ToStdString();
      retained = std::make_shared<celnav::Engine>(options);
    } catch (const std::exception& e) {
      failure = e.what();
    }
  }
  if (!retained && reason) *reason = "Compact data unavailable: " + failure;
  return retained;
}
celnav::Request CompactRequest(
    const wxString& body, const wxDateTime& time, double override_dut1,
    const std::shared_ptr<const eclipse::Dut1Table>& update, bool instant) {
  const auto d = TimeData(time, update, instant);
  celnav::Request r;
  r.body = body.ToStdString();
  r.utc = CompactUtc(time, instant);
  r.dut1_seconds = std::isfinite(override_dut1) ? override_dut1
                   : d.available                ? d.seconds
                                                : 0.0;
  r.tai_minus_utc = std::isfinite(d.tai_minus_utc)
                        ? d.tai_minus_utc
                        : eclipse::TaiMinusUtcSeconds(Calendar(time, instant));
  r.venus_phase = body == "Venus";
  return r;
}
bool TryCompactNavigationSample(const wxString& body, const wxDateTime& time,
                                De440NavigationSample* sample,
                                std::string* reason, double override_dut1,
                                celnav::Result* output, double latitude,
                                double longitude, double height, bool instant) {
  if (!sample || !time.IsValid()) {
    if (reason) *reason = "Invalid compact request";
    return false;
  }
  const auto engine = CompactEngine(reason);
  if (!engine) return false;
  try {
    const auto coverage =
        engine->CheckCoverage(body.ToStdString(), CompactUtc(time, instant),
                              CompactUtc(time, instant));
    if (!coverage.supported) {
      if (reason) *reason = coverage.reason;
      return false;
    }
    const auto update = eclipse::GetDut1Update();
    auto request = CompactRequest(body, time, override_dut1, update, instant);
    request.latitude_deg = latitude;
    request.longitude_deg = longitude;
    request.height_m = height;
    auto result = engine->Evaluate(request);
    const auto dated = TimeData(time, update, instant);
    result.epoch.dut1_available =
        std::isfinite(override_dut1) || dated.available;
    De440NavigationSample value;
    value.gha_deg = result.gha_deg;
    value.declination_deg = result.declination_deg;
    value.aries_gha_deg = result.aries_gha_deg;
    value.range_km = result.distance_km;
    value.horizontal_parallax_deg = result.horizontal_parallax_deg;
    value.semidiameter_deg = result.geocentric_semidiameter_deg;
    value.dut1_seconds = result.epoch.dut1_seconds;
    value.dut1_available = result.epoch.dut1_available;
    value.tai_minus_utc_seconds = result.epoch.tai_minus_utc;
    value.venus_phase_corrected = request.venus_phase;
    if (body == "Sun")
      value.sun_range_au = result.distance_km / kAuKm;
    else if (!output) {
      request.body = "Sun";
      request.venus_phase = false;
      value.sun_range_au = engine->Evaluate(request).distance_km / kAuKm;
    }
    *sample = value;
    if (output) *output = std::move(result);
    return true;
  } catch (const std::exception& e) {
    if (reason) *reason = e.what();
    return false;
  }
}

EnhancedLunarProvider SelectEnhancedLunarProvider(
    const wxString& body, const wxDateTime& reference, double first,
    double last, bool allow_de, bool allow_compact,
    std::shared_ptr<const eclipse::Dut1Table> update) {
  EnhancedLunarProvider selected;
  const wxDateTime reference_instant = UtcDateTime::ToInstant(reference);
  const auto add = [](const wxDateTime& t, double seconds) {
    return t + wxTimeSpan::Milliseconds(
                   static_cast<long long>(std::llround(seconds * 1000.0)));
  };
  const wxDateTime start = add(reference_instant, first);
  const wxDateTime end = add(reference_instant, last);
  const int target = Target(body);
  if (allow_de && target && start.IsValid() &&
      Calendar(start, true).year >= 1972) {
    // Keep a separately opened, verified stream for the lifetime of this
    // callback and any retained observer callback. Updating the pack cannot
    // mutate this session's handle; streams are serialized across threads.
    const wxString path = LunarKernelPath();
    const auto status = eclipse::VerifyDe440s(path.ToStdString());
    auto context = std::make_shared<LunarKernel>();
    std::string error;
    if (status.valid && context->kernel.Open(path.ToStdString(), &error)) {
      auto callback = [context, body, target, reference_instant, update, add](
                          double seconds,
                          lunar_distance::EphemerisSample* sample,
                          std::string* reason) {
        if (!sample) return false;
        try {
          const auto time = add(reference_instant, seconds);
          const auto dated = TimeData(time, update, true);
          const auto utc = Calendar(time, true);
          const double tai = std::isfinite(dated.tai_minus_utc)
                                 ? dated.tai_minus_utc
                                 : eclipse::TaiMinusUtcSeconds(utc);
          eclipse::NavigationEpoch epoch;
          if (!eclipse::MakeNavigationEpoch(utc, dated.seconds, tai, 0, 0,
                                            &epoch, reason))
            return false;
          eclipse::NavigationGeocentricState moon, other;
          std::lock_guard<std::mutex> lock(context->mutex);
          if (!eclipse::GeocentricNavigationState(context->kernel, 301, epoch,
                                                  &moon, reason) ||
              !eclipse::GeocentricNavigationState(context->kernel, target,
                                                  epoch, &other, reason))
            return false;
          SetGeometry(sample, moon.gha_deg, moon.declination_deg,
                      moon.distance_km, other.gha_deg, other.declination_deg,
                      other.distance_km, body == "Sun");
          SetTime(sample, dated);
          sample->observer_direction = [context, target, epoch](
                                           double lat, double lon,
                                           double height, bool is_moon,
                                           double* alt, double* az,
                                           double* sd) {
            std::lock_guard<std::mutex> lock(context->mutex);
            std::string error;
            double range;
            const int id = is_moon ? 301 : target;
            if (!eclipse::ObserverApparentTargetDirection(
                    context->kernel, id, epoch.et_seconds, epoch.orientation,
                    lat, lon, height, alt, az, &range, &error, id == 299))
              return false;
            const double radius = id == 301  ? 1737.4
                                  : id == 10 ? 695700.0
                                             : 0.0;
            *sd = radius ? std::asin(radius / range) / kRad : 0.0;
            return true;
          };
          return true;
        } catch (const std::exception& e) {
          if (reason) *reason = e.what();
          return false;
        }
      };
      lunar_distance::EphemerisSample a, b;
      if (callback(first, &a, &error) && callback(last, &b, &error)) {
        selected.ephemeris = callback;
        selected.used_de440 = true;
        return selected;
      }
    }
  }
  if (allow_compact) {
    const auto engine = CompactEngine(&selected.fallback_reason);
    if (engine) {
      try {
        for (const auto& name : {std::string("Moon"), body.ToStdString()}) {
          const auto coverage = engine->CheckCoverage(
              name, CompactUtc(start, true), CompactUtc(end, true));
          if (!coverage.supported) {
            selected.fallback_reason = coverage.reason;
            return selected;
          }
        }
        selected.ephemeris = [engine, body, reference_instant, update, add](
                                 double seconds,
                                 lunar_distance::EphemerisSample* sample,
                                 std::string* reason) {
          if (!sample) return false;
          try {
            const auto time = add(reference_instant, seconds);
            const auto dated = TimeData(time, update, true);
            auto moon_request = CompactRequest("Moon", time, NAN, update, true);
            auto other_request = CompactRequest(body, time, NAN, update, true);
            const auto values =
                engine->EvaluateMany({moon_request, other_request});
            const auto& m = values[0];
            const auto& b = values[1];
            SetGeometry(sample, m.gha_deg, m.declination_deg, m.distance_km,
                        b.gha_deg, b.declination_deg, b.distance_km,
                        body == "Sun");
            SetTime(sample, dated);
            // The qualified WGS84 forward solver applies exact vector
            // parallax and limb/refraction corrections to these geocentric
            // directions. Do not apply compact's topocentric outputs twice.
            return true;
          } catch (const std::exception& e) {
            if (reason) *reason = e.what();
            return false;
          }
        };
        selected.used_compact = true;
      } catch (const std::exception& e) {
        selected.fallback_reason = e.what();
      }
    }
  } else
    selected.fallback_reason = "Compact disabled in Advanced settings";
  return selected;
}
}  // namespace celestial_navigation
