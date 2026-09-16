#include "eclipse/besselian.h"
#include "eclipse/astronomy.h"
#include "eclipse/engine.h"
#include "eclipse/data_pack.h"
#include "eclipse/geometry.h"
#include "eclipse/spk.h"
#include "eclipse/pck.h"
#include "eclipse/lunar_limb.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int failures = 0;

void ExpectNear(const std::string& label, double actual, double expected,
                double tolerance) {
  const double error = std::fabs(actual - expected);
  if (error > tolerance) {
    std::cerr << "FAIL " << label << ": expected " << expected << " +/- "
              << tolerance << ", got " << actual << " (error " << error
              << ")\n";
    ++failures;
  }
}

void ExpectTrue(const std::string& label, bool value) {
  if (!value) {
    std::cerr << "FAIL " << label << '\n';
    ++failures;
  }
}

eclipse::EclipseType ParseEclipseType(const std::string& value) {
  if (value == "annular") return eclipse::kAnnularEclipse;
  if (value == "total") return eclipse::kTotalEclipse;
  if (value == "hybrid") return eclipse::kHybridEclipse;
  return eclipse::kPartialEclipse;
}

void TestNasaCenturyCatalog(eclipse::EclipseEngine* engine) {
  std::ifstream input(std::string(ECLIPSE_TEST_SOURCE_DIR) +
                      "/nasa_catalog_1850_2100.csv");
  if (!input || !engine) {
    std::cerr << "FAIL opening offline NASA century-catalog fixture\n";
    ++failures;
    return;
  }
  std::string line;
  std::getline(input, line);
  struct Reference {
    int year, month, day;
    eclipse::EclipseType type;
    double td_seconds;
    double magnitude;
  };
  std::vector<Reference> references;
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::string date, type, td, magnitude;
    if (!std::getline(row, date, ',') || !std::getline(row, type, ',') ||
        !std::getline(row, td, ',') || !std::getline(row, magnitude)) {
      std::cerr << "FAIL parsing NASA century-catalog fixture\n";
      ++failures;
      return;
    }
    Reference reference;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &reference.year, &reference.month,
                    &reference.day) != 3) {
      std::cerr << "FAIL parsing NASA catalog date\n";
      ++failures;
      return;
    }
    reference.type = ParseEclipseType(type);
    reference.td_seconds = std::atof(td.c_str());
    reference.magnitude = std::atof(magnitude.c_str());
    references.push_back(reference);
  }

  eclipse::CalendarDateTime start;
  eclipse::CalendarDateTime end;
  start.year = 1850;
  end.year = 2101;
  double start_jd = 0.0, end_jd = 0.0;
  std::string error;
  std::vector<eclipse::EclipseEvent> events;
  if (!eclipse::CalendarToJulianDate(start, &start_jd, &error) ||
      !eclipse::CalendarToJulianDate(end, &end_jd, &error) ||
      !engine->FindEvents(start_jd, end_jd, &events, &error)) {
    std::cerr << "FAIL NASA century-catalog replay: " << error << '\n';
    ++failures;
    return;
  }
  ExpectTrue("NASA catalog has 571 reference events",
             references.size() == 571u);
  ExpectTrue("DE440 discovers every 1850-2100 NASA event",
             events.size() == references.size());
  if (events.size() != references.size()) return;
  double maximum_time_error = 0.0;
  double maximum_magnitude_error = 0.0;
  bool dates_and_types_match = true;
  for (std::size_t index = 0; index < events.size(); ++index) {
    const eclipse::CalendarDateTime date =
        eclipse::JulianDateToCalendar(events[index].maximum_tt_jd);
    const Reference& reference = references[index];
    dates_and_types_match =
        dates_and_types_match && date.year == reference.year &&
        date.month == reference.month && date.day == reference.day &&
        events[index].type == reference.type;
    const double seconds =
        date.hour * 3600.0 + date.minute * 60.0 + date.second;
    maximum_time_error =
        std::max(maximum_time_error, std::fabs(seconds - reference.td_seconds));
    maximum_magnitude_error =
        std::max(maximum_magnitude_error,
                 std::fabs(events[index].magnitude - reference.magnitude));
  }
  ExpectTrue("all NASA catalog dates and classifications match",
             dates_and_types_match);
  ExpectTrue("all NASA greatest-eclipse TD values agree within one second",
             maximum_time_error < 1.0);
  ExpectTrue("all NASA catalog magnitudes agree within 0.0005",
             maximum_magnitude_error < 0.0005);
}

void TestPolynomial() {
  const eclipse::Polynomial3 p(1.0, 2.0, 3.0, 4.0);
  ExpectNear("polynomial", p.Evaluate(2.0), 49.0, 1e-14);
  ExpectNear("polynomial derivative", p.Derivative(2.0), 62.0, 1e-14);
}

void TestGreatestEclipseCentralLine() {
  const eclipse::BesselianElements reference =
      eclipse::Nasa2027Aug02Reference();
  // NASA: greatest eclipse at 10:06:37.7 UT, 25d30.3'N 033d11.0'E.
  const double ut_hours = 10.0 + 6.0 / 60.0 + 37.7 / 3600.0;
  const double tt_hours = ut_hours + reference.delta_t_seconds / 3600.0;
  const double tt_jd = 2461619.5 + tt_hours / 24.0;
  const eclipse::EvaluatedElements evaluated =
      eclipse::Evaluate(reference, tt_jd);
  eclipse::GeoPoint point;
  if (!eclipse::CentralLinePosition(evaluated, eclipse::ReferenceEllipsoid(),
                                    &point)) {
    std::cerr << "FAIL greatest eclipse: shadow axis missed WGS 84\n";
    ++failures;
    return;
  }

  // NASA rounds its path coordinates to 0.1 arcminute. Allow half that unit
  // plus a small allowance for the published polynomial precision.
  // NASA's web table is rounded to 0.1 arcminute while its displayed
  // Besselian coefficients are themselves truncated.  0.07 arcminute covers
  // the combined representation error without concealing a kilometre error.
  const double tolerance_deg = 0.0012;
  ExpectNear("greatest latitude", point.latitude_deg, 25.0 + 30.3 / 60.0,
             tolerance_deg);
  ExpectNear("greatest longitude", point.longitude_deg, 33.0 + 11.0 / 60.0,
             tolerance_deg);
}

void TestOptionalDe440Kernel() {
  const char* path = std::getenv("ECLIPSE_DE440_PATH");
  if (!path || !*path) return;
  const eclipse::DataPackStatus data_status = eclipse::VerifyDe440s(path);
  if (!data_status.valid) {
    std::cerr << "FAIL verifying DE440s data pack: " << data_status.error
              << '\n';
    ++failures;
    return;
  }
  eclipse::SpkKernel kernel;
  std::string error;
  if (!kernel.Open(path, &error)) {
    std::cerr << "FAIL opening DE440: " << error << '\n';
    ++failures;
    return;
  }
  if (kernel.segments().size() != 14u) {
    std::cerr << "FAIL DE440 segment count: expected 14, got "
              << kernel.segments().size() << '\n';
    ++failures;
  }

  // 2027-08-02 is comfortably inside de440s coverage. These broad physical
  // bounds detect corrupt parsing independently of a reference implementation.
  const double et = (2461619.9221 - 2451545.0) * 86400.0;
  eclipse::Vector3 moon;
  eclipse::Vector3 sun;
  if (!kernel.Position(301, 399, et, &moon, &error) ||
      !kernel.Position(10, 399, et, &sun, &error)) {
    std::cerr << "FAIL evaluating DE440: " << error << '\n';
    ++failures;
    return;
  }
  if (moon.Norm() < 350000.0 || moon.Norm() > 410000.0) {
    std::cerr << "FAIL implausible Moon distance: " << moon.Norm() << " km\n";
    ++failures;
  }
  if (sun.Norm() < 145000000.0 || sun.Norm() > 153000000.0) {
    std::cerr << "FAIL implausible Sun distance: " << sun.Norm() << " km\n";
    ++failures;
  }

  // One-time reference captured from the official JPL Horizons API for
  // JD(TDB) 2461619.9221, geometric ICRF vectors, Earth centre. Horizons
  // currently identifies its source as DE441; the 2027 Sun/Earth/Moon solution
  // agrees with DE440 at the precision tested here.
  ExpectNear("DE440 Moon X", moon.x, -227532.4744821665, 0.01);
  ExpectNear("DE440 Moon Y", moon.y, 252500.4995097922, 0.01);
  ExpectNear("DE440 Moon Z", moon.z, 110435.3940965833, 0.01);
  ExpectNear("DE440 Sun X", sun.x, -96645718.62105395, 0.01);
  ExpectNear("DE440 Sun Y", sun.y, 107436416.8997548, 0.01);
  ExpectNear("DE440 Sun Z", sun.z, 46571895.59959871, 0.01);

  eclipse::SolarLunarState astrometric;
  if (!eclipse::AstrometricPosition(kernel, 301, 399, et,
                                    &astrometric.moon_from_earth_km, &error) ||
      !eclipse::AstrometricPosition(kernel, 10, 399, et,
                                    &astrometric.sun_from_earth_km, &error)) {
    std::cerr << "FAIL evaluating astrometric DE440 state: " << error << '\n';
    ++failures;
    return;
  }
  eclipse::EarthOrientation orientation;
  orientation.tt_jd = 2461619.9221;
  orientation.ut1_jd = orientation.tt_jd - 71.7 / 86400.0;
  eclipse::GeoPoint shadow_axis;
  if (!eclipse::ShadowAxisPosition(astrometric, orientation,
                                   eclipse::ReferenceEllipsoid(),
                                   &shadow_axis)) {
    std::cerr << "FAIL DE440 2027 shadow axis misses WGS 84\n";
    ++failures;
    return;
  }
  // NASA's rounded greatest-eclipse location is 25d30.3'N, 33d11.0'E.
  ExpectNear("DE440 2027 greatest latitude", shadow_axis.latitude_deg,
             25.0 + 30.3 / 60.0, 0.003);
  ExpectNear("DE440 2027 greatest longitude", shadow_axis.longitude_deg,
             33.0 + 11.0 / 60.0, 0.004);

  eclipse::EclipseEngine engine;
  if (!engine.OpenEphemeris(path, &error)) {
    std::cerr << "FAIL engine opening DE440: " << error << '\n';
    ++failures;
    return;
  }

  TestNasaCenturyCatalog(&engine);

  // Verify independent event discovery. NASA's greatest-eclipse TT is
  // 10:07:49.4 and the reference Delta-T is 71.7 seconds.
  eclipse::CalendarDateTime start_date;
  start_date.year = 2027;
  eclipse::CalendarDateTime end_date;
  end_date.year = 2028;
  double start_jd = 0.0;
  double end_jd = 0.0;
  if (!eclipse::CalendarToJulianDate(start_date, &start_jd, &error) ||
      !eclipse::CalendarToJulianDate(end_date, &end_jd, &error)) {
    std::cerr << "FAIL converting regression dates: " << error << '\n';
    ++failures;
    return;
  }
  std::vector<eclipse::EclipseEvent> events;
  if (!engine.FindEvents(start_jd, end_jd, &events, &error, 71.7)) {
    std::cerr << "FAIL discovering 2027 eclipses: " << error << '\n';
    ++failures;
    return;
  }
  ExpectTrue("two eclipses discovered in 2027", events.size() == 2u);
  if (events.size() == 2u) {
    ExpectTrue("February 2027 eclipse is annular",
               events[0].type == eclipse::kAnnularEclipse);
    ExpectTrue("August 2027 eclipse is total",
               events[1].type == eclipse::kTotalEclipse);
    ExpectNear("2027 greatest TT", events[1].maximum_tt_jd, 2461619.922099537,
               3.0 / 86400.0);
  }

  // NASA's published path row at 10:00 UT. The limits in the table are
  // rounded to 0.1 arcminute and its conventional lunar-radius corrections
  // differ slightly from a physical cone, so sub-kilometre tolerances are
  // neither meaningful nor expected here.
  const double row_tt = 2461619.5 + (10.0 + 71.7 / 3600.0) / 24.0;
  eclipse::ShadowFootprint footprint;
  if (!engine.Footprint(row_tt, 71.7, 7200, &footprint, &error)) {
    std::cerr << "FAIL evaluating NASA 10:00 path row: " << error << '\n';
    ++failures;
    return;
  }
  eclipse::GeoPoint before;
  eclipse::GeoPoint after;
  eclipse::SolarLunarState adjacent_state;
  eclipse::EarthOrientation adjacent_orientation;
  if (!engine.State(row_tt - 1.0 / 86400.0, 71.7, &adjacent_state,
                    &adjacent_orientation, &error) ||
      !eclipse::ShadowAxisPosition(adjacent_state, adjacent_orientation,
                                   eclipse::ReferenceEllipsoid(), &before) ||
      !engine.State(row_tt + 1.0 / 86400.0, 71.7, &adjacent_state,
                    &adjacent_orientation, &error) ||
      !eclipse::ShadowAxisPosition(adjacent_state, adjacent_orientation,
                                   eclipse::ReferenceEllipsoid(), &after)) {
    std::cerr << "FAIL evaluating adjacent 10:00 axes: " << error << '\n';
    ++failures;
    return;
  }
  eclipse::GeoPoint north;
  eclipse::GeoPoint south;
  if (!eclipse::SelectCrossTrackLimits(footprint, before, after, &north,
                                       &south)) {
    std::cerr << "FAIL selecting 10:00 path limits\n";
    ++failures;
    return;
  }
  if (north.latitude_deg < south.latitude_deg) std::swap(north, south);
  ExpectNear("2027 10:00 north latitude", north.latitude_deg,
             27.0 + 51.4 / 60.0, 0.006);
  ExpectNear("2027 10:00 north longitude", north.longitude_deg,
             31.0 + 44.0 / 60.0, 0.006);
  ExpectNear("2027 10:00 south latitude", south.latitude_deg,
             25.0 + 55.3 / 60.0, 0.006);
  ExpectNear("2027 10:00 south longitude", south.longitude_deg,
             30.0 + 18.2 / 60.0, 0.006);
  ExpectNear("2027 10:00 centre latitude", footprint.axis.latitude_deg,
             26.0 + 53.3 / 60.0, 0.006);
  ExpectNear("2027 10:00 centre longitude", footprint.axis.longitude_deg,
             31.0 + 0.8 / 60.0, 0.006);
  ExpectNear("2027 10:00 path width", eclipse::SurfaceDistanceKm(north, south),
             257.0, 1.0);

  // Local circumstances at NASA's greatest-eclipse location. Published
  // duration is 6m22.6s; the spherical-limb solution agrees within a second.
  eclipse::EclipseEvent august;
  august.type = eclipse::kTotalEclipse;
  august.maximum_tt_jd = 2461619.9221;
  august.delta_t_seconds = 71.7;
  eclipse::LocalContacts contacts;
  if (!engine.SolveLocalContacts(
          august, eclipse::GeoPoint(25.0 + 30.3 / 60.0, 33.0 + 11.0 / 60.0),
          0.0, &contacts, &error)) {
    std::cerr << "FAIL solving local contacts: " << error << '\n';
    ++failures;
    return;
  }
  ExpectTrue("2027 greatest location is total",
             contacts.type == eclipse::kTotalEclipse);
  ExpectNear("2027 central duration", contacts.central_duration_seconds, 382.6,
             1.0);
  ExpectNear("2027 local maximum UT", contacts.maximum.tt_jd - 71.7 / 86400.0,
             2461619.921269676, 1.0 / 86400.0);

  struct CentralRegression {
    int year;
    double delta_t;
    double maximum_tt;
    double latitude;
    double longitude;
  } regressions[] = {
      // NASA/GSFC detailed Besselian pages, VSOP87/ELP2000-85 references.
      {2024, 70.6, 2460409.262835, 25.0 + 17.2 / 60.0, -(104.0 + 8.3 / 60.0)},
      {2026, 71.4, 2461265.241032, 65.0 + 13.5 / 60.0, -(25.0 + 13.7 / 60.0)}};
  for (std::size_t regression_index = 0;
       regression_index < sizeof(regressions) / sizeof(regressions[0]);
       ++regression_index) {
    const CentralRegression& reference = regressions[regression_index];
    eclipse::CalendarDateTime regression_start;
    eclipse::CalendarDateTime regression_end;
    regression_start.year = reference.year;
    regression_end.year = reference.year + 1;
    double regression_start_jd = 0.0, regression_end_jd = 0.0;
    eclipse::CalendarToJulianDate(regression_start, &regression_start_jd,
                                  &error);
    eclipse::CalendarToJulianDate(regression_end, &regression_end_jd, &error);
    std::vector<eclipse::EclipseEvent> regression_events;
    if (!engine.FindEvents(regression_start_jd, regression_end_jd,
                           &regression_events, &error, reference.delta_t)) {
      std::cerr << "FAIL finding " << reference.year << " regression: " << error
                << '\n';
      ++failures;
      continue;
    }
    const eclipse::EclipseEvent* total_event = NULL;
    for (std::size_t index = 0; index < regression_events.size(); ++index)
      if (regression_events[index].type == eclipse::kTotalEclipse)
        total_event = &regression_events[index];
    ExpectTrue("regression total eclipse discovered", total_event != NULL);
    if (!total_event) continue;
    ExpectNear("regression greatest TT", total_event->maximum_tt_jd,
               reference.maximum_tt, 3.0 / 86400.0);
    ExpectNear("regression greatest latitude",
               total_event->greatest_position.latitude_deg, reference.latitude,
               0.01);
    ExpectNear("regression greatest longitude",
               total_event->greatest_position.longitude_deg,
               reference.longitude, 0.01);
  }

  // Discovery guards for events which the old narrow conjunction prefilter
  // could miss between six-hour samples. NASA's century catalog lists four
  // partial eclipses in 2029 and a high-latitude total on 2072-09-12.
  struct DiscoveryRegression {
    int year;
    int count;
    eclipse::EclipseType type;
    int month;
    int day;
  };
  const DiscoveryRegression discovery[] = {
      {2029, 4, eclipse::kPartialEclipse, 12, 5},
      {2072, 2, eclipse::kTotalEclipse, 9, 12}};
  for (std::size_t test_index = 0;
       test_index < sizeof(discovery) / sizeof(discovery[0]); ++test_index) {
    eclipse::CalendarDateTime year_start;
    eclipse::CalendarDateTime year_end;
    year_start.year = discovery[test_index].year;
    year_end.year = discovery[test_index].year + 1;
    double year_start_jd = 0.0, year_end_jd = 0.0;
    eclipse::CalendarToJulianDate(year_start, &year_start_jd, &error);
    eclipse::CalendarToJulianDate(year_end, &year_end_jd, &error);
    std::vector<eclipse::EclipseEvent> found;
    if (!engine.FindEvents(year_start_jd, year_end_jd, &found, &error)) {
      std::cerr << "FAIL discovery regression: " << error << '\n';
      ++failures;
      continue;
    }
    ExpectTrue("complete annual eclipse discovery",
               static_cast<int>(found.size()) == discovery[test_index].count);
    bool matched = false;
    for (std::size_t event_index = 0; event_index < found.size();
         ++event_index) {
      const eclipse::CalendarDateTime date =
          eclipse::JulianDateToCalendar(found[event_index].maximum_tt_jd);
      if (found[event_index].type == discovery[test_index].type &&
          date.month == discovery[test_index].month &&
          date.day == discovery[test_index].day)
        matched = true;
    }
    ExpectTrue("edge-case eclipse discovered and classified", matched);
  }

  const double contour_values[] = {0.2, 0.5, 0.8, 0.9};
  std::vector<eclipse::MagnitudeContour> contours;
  if (!engine.BuildMagnitudeContours(
          august, std::vector<double>(contour_values, contour_values + 4), 4.0,
          600.0, &contours, &error)) {
    std::cerr << "FAIL building magnitude contours: " << error << '\n';
    ++failures;
    return;
  }
  ExpectTrue("four magnitude contours generated", contours.size() == 4u);
  for (std::size_t index = 0; index < contours.size(); ++index) {
    ExpectTrue("magnitude contour is non-empty",
               !contours[index].segments.empty());
    for (std::size_t segment = 0; segment < contours[index].segments.size();
         ++segment) {
      const eclipse::ContourSegment& item = contours[index].segments[segment];
      ExpectTrue("contour latitude in range",
                 item.first.latitude_deg >= -90.0 &&
                     item.first.latitude_deg <= 90.0 &&
                     item.second.latitude_deg >= -90.0 &&
                     item.second.latitude_deg <= 90.0);
      ExpectTrue("contour longitude in range",
                 item.first.longitude_deg >= -180.0 &&
                     item.first.longitude_deg <= 180.0 &&
                     item.second.longitude_deg >= -180.0 &&
                     item.second.longitude_deg <= 180.0);
    }
  }
}

void TestOptionalLunarPck() {
  const char* path = std::getenv("ECLIPSE_LUNAR_PCK_PATH");
  if (!path || !*path) return;
  eclipse::PckKernel kernel;
  std::string error;
  eclipse::Vector3 axes[3];
  if (!kernel.Open(path, &error) ||
      !kernel.IcrfToBodyFixed(870214852.8, axes, &error)) {
    std::cerr << "FAIL lunar PCK: " << error << '\n';
    ++failures;
    return;
  }
  // Independently captured with official CSPICE N0067 pxform_c from J2000
  // to MOON_PA_DE440 at the same ET epoch.
  ExpectNear("lunar PCK m00", axes[0].x, -0.004980499720, 2e-12);
  ExpectNear("lunar PCK m01", axes[1].x, -0.924323490154, 2e-12);
  ExpectNear("lunar PCK m02", axes[2].x, -0.381577358043, 2e-12);
  ExpectNear("lunar PCK m10", axes[0].y, 0.999792528866, 2e-12);
  ExpectNear("lunar PCK m11", axes[1].y, 0.002933890625, 2e-12);
  ExpectNear("lunar PCK m12", axes[2].y, -0.020156674053, 2e-12);
  ExpectNear("lunar PCK m20", axes[0].z, 0.019750793544, 2e-12);
  ExpectNear("lunar PCK m21", axes[1].z, -0.381598582066, 2e-12);
  ExpectNear("lunar PCK m22", axes[2].z, 0.924117107471, 2e-12);
}

void TestLunarLimbGrid() {
  const std::string path = "/tmp/celestial-eclipse-limb-test.bin";
  std::vector<std::int16_t> offsets(360u * 180u, 0);
  std::string error;
  if (!eclipse::WriteLunarLimbGrid(path, 360, 180, 1737.4, offsets, &error)) {
    std::cerr << "FAIL writing synthetic lunar limb grid: " << error << '\n';
    ++failures;
    return;
  }
  eclipse::LunarLimbGrid grid;
  eclipse::LunarLimbProfile profile;
  if (!grid.Open(path, &error) ||
      !grid.BuildProfile(eclipse::Vector3(1.0, 0.0, 0.0), 360, &profile,
                         &error)) {
    std::cerr << "FAIL building synthetic lunar limb: " << error << '\n';
    ++failures;
    std::remove(path.c_str());
    return;
  }
  ExpectTrue("synthetic limb has 360 bins", profile.radius_km.size() == 360u);
  for (std::size_t index = 0; index < profile.radius_km.size(); ++index)
    ExpectNear("synthetic spherical limb radius", profile.radius_km[index],
               1737.4, 0.25);
  std::remove(path.c_str());
}

void TestOptionalLolaPack() {
  const char* path = std::getenv("ECLIPSE_LOLA_PATH");
  if (!path || !*path) return;
  const eclipse::DataPackStatus status = eclipse::VerifyLola64Pa(path);
  ExpectTrue("verified official LOLA runtime pack", status.valid);
  if (!status.valid) {
    std::cerr << status.error << '\n';
    return;
  }
  eclipse::LunarLimbGrid grid;
  std::string error;
  double radius = 0.0;
  if (!grid.Open(path, &error) ||
      !grid.SupportRadiusKm(eclipse::Vector3(1.0, 0.0, 0.0), &radius, &error)) {
    std::cerr << "FAIL reading official LOLA runtime pack: " << error << '\n';
    ++failures;
    return;
  }
  ExpectTrue("LOLA support radius is physically plausible",
             radius > 1735.0 && radius < 1745.0);
}

}  // namespace

int main() {
  TestPolynomial();
  TestGreatestEclipseCentralLine();
  TestOptionalDe440Kernel();
  TestOptionalLunarPck();
  TestLunarLimbGrid();
  TestOptionalLolaPack();
  if (failures != 0) {
    std::cerr << failures << " eclipse core test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All eclipse core tests passed\n";
  return EXIT_SUCCESS;
}
