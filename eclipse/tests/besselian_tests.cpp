#include "eclipse/besselian.h"
#include "eclipse/astronomy.h"
#include "eclipse/engine.h"
#include "eclipse/data_pack.h"
#include "eclipse/geometry.h"
#include "eclipse/spk.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
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
  if (!eclipse::CentralLinePosition(evaluated,
                                    eclipse::ReferenceEllipsoid(), &point)) {
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
                                     &astrometric.moon_from_earth_km,
                                     &error) ||
      !eclipse::AstrometricPosition(kernel, 10, 399, et,
                                     &astrometric.sun_from_earth_km,
                                     &error)) {
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
    ExpectNear("2027 greatest TT", events[1].maximum_tt_jd,
               2461619.922099537, 3.0 / 86400.0);
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
  if (!eclipse::SelectCrossTrackLimits(footprint, before, after,
                                        &north, &south)) {
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
  ExpectNear("2027 10:00 path width",
             eclipse::SurfaceDistanceKm(north, south), 257.0, 1.0);

  // Local circumstances at NASA's greatest-eclipse location. Published
  // duration is 6m22.6s; the spherical-limb solution agrees within a second.
  eclipse::EclipseEvent august;
  august.type = eclipse::kTotalEclipse;
  august.maximum_tt_jd = 2461619.9221;
  august.delta_t_seconds = 71.7;
  eclipse::LocalContacts contacts;
  if (!engine.SolveLocalContacts(august,
          eclipse::GeoPoint(25.0 + 30.3 / 60.0,
                            33.0 + 11.0 / 60.0),
          0.0, &contacts, &error)) {
    std::cerr << "FAIL solving local contacts: " << error << '\n';
    ++failures;
    return;
  }
  ExpectTrue("2027 greatest location is total",
             contacts.type == eclipse::kTotalEclipse);
  ExpectNear("2027 central duration", contacts.central_duration_seconds,
             382.6, 1.0);
  ExpectNear("2027 local maximum UT",
             contacts.maximum.tt_jd - 71.7 / 86400.0,
             2461619.921269676, 1.0 / 86400.0);
}

}  // namespace

int main() {
  TestPolynomial();
  TestGreatestEclipseCentralLine();
  TestOptionalDe440Kernel();
  if (failures != 0) {
    std::cerr << failures << " eclipse core test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All eclipse core tests passed\n";
  return EXIT_SUCCESS;
}
