#include "eclipse/astronomy.h"
#include "eclipse/spk.h"

extern "C" {
#include "erfa.h"
}

#include <algorithm>
#include <cmath>
#include <limits>

namespace eclipse {

bool AstrometricPosition(const SpkKernel& kernel, std::int32_t target,
                         std::int32_t observer, double reception_et,
                         Vector3* position_km, std::string* error) {
  if (!position_km) return false;
  const double speed_of_light_km_s = 299792.458;
  Vector3 observer_barycentric;
  if (!kernel.Position(observer, 0, reception_et, &observer_barycentric,
                       error)) {
    return false;
  }
  double transmission_et = reception_et;
  Vector3 relative;
  for (int iteration = 0; iteration < 8; ++iteration) {
    Vector3 target_barycentric;
    if (!kernel.Position(target, 0, transmission_et, &target_barycentric,
                         error)) {
      return false;
    }
    relative = target_barycentric - observer_barycentric;
    const double next = reception_et - relative.Norm() / speed_of_light_km_s;
    if (std::fabs(next - transmission_et) < 1e-9) break;
    transmission_et = next;
  }
  *position_km = relative;
  return true;
}

Vector3 IcrfToEarthFixed(const Vector3& icrf,
                         const EarthOrientation& orientation) {
  double matrix[3][3];
  eraC2t06a(2451545.0, orientation.tt_jd - 2451545.0, 2451545.0,
            orientation.ut1_jd - 2451545.0,
            orientation.polar_motion_x_rad,
            orientation.polar_motion_y_rad, matrix);
  return Vector3(matrix[0][0] * icrf.x + matrix[0][1] * icrf.y +
                     matrix[0][2] * icrf.z,
                 matrix[1][0] * icrf.x + matrix[1][1] * icrf.y +
                     matrix[1][2] * icrf.z,
                 matrix[2][0] * icrf.x + matrix[2][1] * icrf.y +
                     matrix[2][2] * icrf.z);
}

bool ShadowAxisPosition(const SolarLunarState& state,
                        const EarthOrientation& orientation,
                        const ReferenceEllipsoid& ellipsoid,
                        GeoPoint* position) {
  if (!position) return false;
  const Vector3 direction_icrf =
      Normalize(state.moon_from_earth_km - state.sun_from_earth_km);
  if (direction_icrf.Norm() == 0.0) return false;

  const Vector3 moon =
      IcrfToEarthFixed(state.moon_from_earth_km, orientation);
  const Vector3 direction = IcrfToEarthFixed(direction_icrf, orientation);
  const double polar_ratio = ellipsoid.polar_ratio();
  const double inverse_polar_squared = 1.0 / (polar_ratio * polar_ratio);

  const double a = direction.x * direction.x + direction.y * direction.y +
                   direction.z * direction.z * inverse_polar_squared;
  const double b = 2.0 * (moon.x * direction.x + moon.y * direction.y +
                          moon.z * direction.z * inverse_polar_squared);
  const double equatorial_radius_km = 6378.137;
  const double c = moon.x * moon.x + moon.y * moon.y +
                   moon.z * moon.z * inverse_polar_squared -
                   equatorial_radius_km * equatorial_radius_km;
  const double discriminant = b * b - 4.0 * a * c;
  if (discriminant < 0.0) return false;

  const double square_root = std::sqrt(discriminant);
  const double root1 = (-b + square_root) / (2.0 * a);
  const double root2 = (-b - square_root) / (2.0 * a);
  double parameter = std::numeric_limits<double>::infinity();
  if (root1 >= 0.0) parameter = std::min(parameter, root1);
  if (root2 >= 0.0) parameter = std::min(parameter, root2);
  if (!std::isfinite(parameter)) return false;

  const Vector3 point = moon + direction * parameter;
  const double longitude = std::atan2(point.y, point.x);
  const double horizontal = std::hypot(point.x, point.y);
  const double latitude =
      std::atan2(point.z, polar_ratio * polar_ratio * horizontal);
  position->latitude_deg = RadiansToDegrees(latitude);
  position->longitude_deg = NormalizeLongitude(RadiansToDegrees(longitude));
  return true;
}

}  // namespace eclipse
