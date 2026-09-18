#include "eclipse/astronomy.h"
#include "eclipse/dut1.h"
#include "eclipse/navigation.h"
#include "eclipse/spk.h"
#include "eclipse/time.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
bool ParseUtc(const std::string& input, eclipse::CalendarDateTime* result) {
  int year, month, day, hour, minute;
  double second;
  int used = 0;
  if (std::sscanf(input.c_str(), "%d-%d-%dT%d:%d:%lf%n", &year, &month,
                  &day, &hour, &minute, &second, &used) != 6 ||
      used != static_cast<int>(input.size()))
    return false;
  result->year = year;
  result->month = month;
  result->day = day;
  result->hour = hour;
  result->minute = minute;
  result->second = second;
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 9) {
    std::cerr << "Usage: navigation-cli KERNEL TARGET UTC LAT LON HEIGHT_M DUT1_S TAI_MINUS_UTC_S\n";
    return 2;
  }
  try {
    const int target = std::stoi(argv[2]);
    if (!eclipse::De440sNavigationTarget(target)) {
      std::cerr << "The compact DE440s kernel does not contain this target centre\n";
      return 2;
    }
    eclipse::CalendarDateTime utc;
    if (!ParseUtc(argv[3], &utc)) {
      std::cerr << "UTC must be YYYY-MM-DDTHH:MM:SS[.sss]\n";
      return 2;
    }
    const double lat = std::stod(argv[4]);
    const double lon = std::stod(argv[5]);
    const double height = std::stod(argv[6]);
    std::string error;
    double utc_jd = 0.0;
    if (!eclipse::CalendarToJulianDate(utc, &utc_jd, &error)) {
      std::cerr << error << '\n';
      return 2;
    }
    const auto table = eclipse::LookupDut1(utc_jd);
    if (std::string(argv[7]) == "auto" && !table.available) {
      std::cerr << "DUT1 unavailable for this UTC; supply an explicit value\n";
      return 2;
    }
    const double dut1 = std::string(argv[7]) == "auto" ?
        table.seconds : std::stod(argv[7]);
    const double tai_minus_utc = std::string(argv[8]) == "auto" ?
        eclipse::TaiMinusUtcSeconds(utc) : std::stod(argv[8]);
    eclipse::SpkKernel kernel;
    if (!kernel.Open(argv[1], &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    eclipse::NavigationEpoch epoch;
    if (!eclipse::MakeNavigationEpoch(utc, dut1, tai_minus_utc, 0.0, 0.0,
                                      &epoch, &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    eclipse::NavigationGeocentricState geo;
    if (!eclipse::GeocentricNavigationState(kernel, target, epoch, &geo,
                                            &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    double altitude = 0.0, azimuth = 0.0, range = 0.0;
    if (!eclipse::ObserverApparentTargetDirection(
            kernel, target, epoch.et_seconds, epoch.orientation,
            lat, lon, height, &altitude, &azimuth, &range, &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    if (!std::isfinite(altitude) || !std::isfinite(azimuth)) return 1;
    std::cout << std::setprecision(15)
              << "{\"target\":" << target
              << ",\"gha_deg\":" << geo.gha_deg
              << ",\"declination_deg\":" << geo.declination_deg
              << ",\"centre_gha_deg\":" << geo.centre_gha_deg
              << ",\"centre_declination_deg\":" << geo.centre_declination_deg
              << ",\"phase_corrected\":" << (geo.phase_corrected ? "true" : "false")
              << ",\"gha_aries_deg\":" << geo.gha_aries_deg
              << ",\"distance_km\":" << geo.distance_km
              << ",\"horizontal_parallax_deg\":" << geo.horizontal_parallax_deg
              << ",\"semidiameter_deg\":" << geo.semidiameter_deg
              << ",\"airless_topocentric_altitude_deg\":" << altitude
              << ",\"airless_topocentric_azimuth_deg\":" << azimuth
              << ",\"observer_range_km\":" << range
              << ",\"dut1_seconds\":" << dut1
              << ",\"tai_minus_utc_seconds\":" << tai_minus_utc
              << ",\"dut1_quality\":\"" << (table.available ? table.quality : '?')
              << "\"}\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 2;
  }
  return 0;
}
