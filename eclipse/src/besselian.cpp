#include "eclipse/besselian.h"

#include <algorithm>
#include <cmath>

namespace eclipse {
namespace {

struct Vec3 {
  double x;
  double y;
  double z;
};

Vec3 FundamentalToEarthEquatorial(const EvaluatedElements& elements, double xi,
                                  double eta, double zeta) {
  const double sin_d = std::sin(elements.declination_rad);
  const double cos_d = std::cos(elements.declination_rad);

  Vec3 result;
  result.x = zeta * cos_d - eta * sin_d;
  result.y = xi;
  result.z = eta * cos_d + zeta * sin_d;
  return result;
}

double EllipsoidEquation(const EvaluatedElements& elements,
                         const ReferenceEllipsoid& ellipsoid, double zeta) {
  const Vec3 point =
      FundamentalToEarthEquatorial(elements, elements.x, elements.y, zeta);
  const double polar_ratio = ellipsoid.polar_ratio();
  return point.x * point.x + point.y * point.y +
         point.z * point.z / (polar_ratio * polar_ratio) - 1.0;
}

}  // namespace

double Polynomial3::Evaluate(double hours) const {
  return ((c3 * hours + c2) * hours + c1) * hours + c0;
}

double Polynomial3::Derivative(double hours) const {
  return (3.0 * c3 * hours + 2.0 * c2) * hours + c1;
}

BesselianElements::BesselianElements()
    : reference_tt_jd(0.0), delta_t_seconds(0.0), tan_f1(0.0), tan_f2(0.0) {}

EvaluatedElements Evaluate(const BesselianElements& elements,
                           double terrestrial_time_jd) {
  const double hours = (terrestrial_time_jd - elements.reference_tt_jd) * 24.0;
  EvaluatedElements result;
  result.x = elements.x.Evaluate(hours);
  result.y = elements.y.Evaluate(hours);
  result.declination_rad =
      DegreesToRadians(elements.declination_deg.Evaluate(hours));
  result.penumbral_radius = elements.penumbral_radius.Evaluate(hours);
  result.umbral_radius = elements.umbral_radius.Evaluate(hours);
  // Published Besselian polynomials use TT/TDT as their common argument, but
  // longitude depends on UT1. Correct the ephemeris hour angle by Earth's
  // sidereal rotation during Delta-T (1.00273790935 sidereal/solar days).
  const double delta_t_rotation_deg =
      elements.delta_t_seconds * 360.0 * 1.00273790935 / 86400.0;
  result.hour_angle_rad = DegreesToRadians(
      elements.hour_angle_deg.Evaluate(hours) - delta_t_rotation_deg);
  result.tan_f1 = elements.tan_f1;
  result.tan_f2 = elements.tan_f2;
  return result;
}

bool CentralLinePosition(const EvaluatedElements& elements,
                         const ReferenceEllipsoid& ellipsoid,
                         GeoPoint* position) {
  if (!position) return false;

  // Substitution of the shadow-axis line (xi=x, eta=y) into the oblate
  // ellipsoid produces a quadratic in zeta. Calculate its coefficients from
  // three exact samples; this also keeps the frame transformation in one
  // implementation.
  const double f0 = EllipsoidEquation(elements, ellipsoid, 0.0);
  const double fp = EllipsoidEquation(elements, ellipsoid, 1.0);
  const double fm = EllipsoidEquation(elements, ellipsoid, -1.0);
  const double a = 0.5 * (fp + fm) - f0;
  const double b = 0.5 * (fp - fm);
  const double c = f0;
  const double discriminant = b * b - 4.0 * a * c;
  if (a <= 0.0 || discriminant < 0.0) return false;

  // The positive root is the surface facing the Sun and Moon.
  const double sqrt_discriminant = std::sqrt(discriminant);
  const double root1 = (-b + sqrt_discriminant) / (2.0 * a);
  const double root2 = (-b - sqrt_discriminant) / (2.0 * a);
  const double zeta = std::max(root1, root2);

  const Vec3 point =
      FundamentalToEarthEquatorial(elements, elements.x, elements.y, zeta);
  const double geocentric_longitude = std::atan2(point.y, point.x);
  const double longitude = geocentric_longitude - elements.hour_angle_rad;

  // Convert ECEF coordinates on WGS 84 to geodetic latitude. Because the
  // coordinates already lie on the ellipsoid, the closed-form normal relation
  // tan(phi) = z / ((1-e^2) * p) is sufficient.
  const double polar_ratio = ellipsoid.polar_ratio();
  const double one_minus_e2 = polar_ratio * polar_ratio;
  const double horizontal = std::hypot(point.x, point.y);
  const double latitude = std::atan2(point.z, one_minus_e2 * horizontal);

  position->latitude_deg = RadiansToDegrees(latitude);
  position->longitude_deg = NormalizeLongitude(RadiansToDegrees(longitude));
  return true;
}

BesselianElements Nasa2027Aug02Reference() {
  BesselianElements result;
  // 2027-08-02 10:00:00.0 TT (TDT), JD 2461619.9166666665.
  result.reference_tt_jd = 2461619.9166666665;
  result.delta_t_seconds = 71.7;
  result.x = Polynomial3(-0.019645, 0.5447105, -0.0000444, -0.0000091);
  result.y = Polynomial3(0.160063, -0.2111569, -0.0001217, 0.0000037);
  result.declination_deg = Polynomial3(17.76247, -0.010181, -0.000004, 0.0);
  result.penumbral_radius = Polynomial3(0.530596, 0.0000138, -0.0000128, 0.0);
  result.umbral_radius = Polynomial3(-0.015464, 0.0000137, -0.0000128, 0.0);
  result.hour_angle_deg = Polynomial3(328.42249, 15.002093, 0.0, 0.0);
  result.tan_f1 = 0.0046064;
  result.tan_f2 = 0.0045834;
  return result;
}

}  // namespace eclipse
