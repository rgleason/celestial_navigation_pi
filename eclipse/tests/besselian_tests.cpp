#include "eclipse/besselian.h"
#include "eclipse/astronomy.h"
#include "eclipse/spk.h"

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
