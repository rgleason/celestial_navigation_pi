#include "eclipse/geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace eclipse {
namespace {

Vector3 GeodeticEarthFixed(const GeoPoint& point, double height_metres,
                           const ReferenceEllipsoid& ellipsoid,
                           double equatorial_radius_km) {
  const double latitude = DegreesToRadians(point.latitude_deg);
  const double longitude = DegreesToRadians(point.longitude_deg);
  const double sin_latitude = std::sin(latitude);
  const double cos_latitude = std::cos(latitude);
  const double e2 = 1.0 - ellipsoid.polar_ratio() * ellipsoid.polar_ratio();
  const double normal =
      equatorial_radius_km / std::sqrt(1.0 - e2 * sin_latitude * sin_latitude);
  const double height_km = height_metres / 1000.0;
  return Vector3((normal + height_km) * cos_latitude * std::cos(longitude),
                 (normal + height_km) * cos_latitude * std::sin(longitude),
                 (normal * (1.0 - e2) + height_km) * sin_latitude);
}

bool RayEllipsoidIntersection(const Vector3& origin, const Vector3& direction,
                              const ReferenceEllipsoid& ellipsoid,
                              double equatorial_radius_km, bool choose_far,
                              Vector3* point) {
  const double polar_radius_km = equatorial_radius_km * ellipsoid.polar_ratio();
  const double inv_a2 = 1.0 / (equatorial_radius_km * equatorial_radius_km);
  const double inv_b2 = 1.0 / (polar_radius_km * polar_radius_km);
  const double a =
      (direction.x * direction.x + direction.y * direction.y) * inv_a2 +
      direction.z * direction.z * inv_b2;
  const double b =
      2.0 * ((origin.x * direction.x + origin.y * direction.y) * inv_a2 +
             origin.z * direction.z * inv_b2);
  const double c = (origin.x * origin.x + origin.y * origin.y) * inv_a2 +
                   origin.z * origin.z * inv_b2 - 1.0;
  const double discriminant = b * b - 4.0 * a * c;
  if (discriminant < 0.0) return false;
  const double root = std::sqrt(discriminant);
  const double t1 = (-b - root) / (2.0 * a);
  const double t2 = (-b + root) / (2.0 * a);
  double t = choose_far ? -std::numeric_limits<double>::infinity()
                        : std::numeric_limits<double>::infinity();
  if (t1 >= 0.0) t = choose_far ? std::max(t, t1) : std::min(t, t1);
  if (t2 >= 0.0) t = choose_far ? std::max(t, t2) : std::min(t, t2);
  if (!std::isfinite(t)) return false;
  if (point) *point = origin + direction * t;
  return true;
}

GeoPoint EarthFixedToGeodetic(const Vector3& point,
                              const ReferenceEllipsoid& ellipsoid) {
  const double longitude = std::atan2(point.y, point.x);
  const double horizontal = std::hypot(point.x, point.y);
  const double latitude = std::atan2(
      point.z, ellipsoid.polar_ratio() * ellipsoid.polar_ratio() * horizontal);
  return GeoPoint(RadiansToDegrees(latitude),
                  NormalizeLongitude(RadiansToDegrees(longitude)));
}

double CircleOverlap(double r1, double r2, double distance) {
  if (distance >= r1 + r2) return 0.0;
  if (distance <= std::fabs(r1 - r2)) {
    const double smaller = std::min(r1, r2);
    return 3.14159265358979323846 * smaller * smaller;
  }
  const double c1 =
      std::max(-1.0, std::min(1.0, (distance * distance + r1 * r1 - r2 * r2) /
                                       (2.0 * distance * r1)));
  const double c2 =
      std::max(-1.0, std::min(1.0, (distance * distance + r2 * r2 - r1 * r1) /
                                       (2.0 * distance * r2)));
  const double term =
      std::max(0.0, (-distance + r1 + r2) * (distance + r1 - r2) *
                        (distance - r1 + r2) * (distance + r1 + r2));
  return r1 * r1 * std::acos(c1) + r2 * r2 * std::acos(c2) -
         0.5 * std::sqrt(term);
}

}  // namespace

ShadowFootprint CentralShadowFootprint(const SolarLunarState& state,
                                       const EarthOrientation& orientation,
                                       const ReferenceEllipsoid& ellipsoid,
                                       const PhysicalConstants& constants,
                                       int angular_samples) {
  ShadowFootprint result;
  result.central =
      ShadowAxisPosition(state, orientation, ellipsoid, &result.axis);
  if (!result.central || angular_samples < 8) return result;

  const Vector3 sun = IcrfToEarthFixed(state.sun_from_earth_km, orientation);
  const Vector3 moon = IcrfToEarthFixed(state.moon_from_earth_km, orientation);
  const Vector3 away_from_sun = Normalize(moon - sun);
  const double separation = (moon - sun).Norm();
  const double apex_distance =
      constants.moon_radius_km * separation /
      (constants.sun_radius_km - constants.moon_radius_km);
  const Vector3 apex = moon + away_from_sun * apex_distance;
  const double sin_half_angle =
      (constants.sun_radius_km - constants.moon_radius_km) / separation;
  const double half_angle = std::asin(sin_half_angle);

  // A hybrid eclipse occurs when the cone apex crosses the curved Earth:
  // classification must therefore use the actual near-side axis
  // intersection, not the Earth's centre.
  Vector3 axis_intersection;
  if (!RayEllipsoidIntersection(moon, away_from_sun, ellipsoid,
                                constants.earth_equatorial_radius_km, false,
                                &axis_intersection)) {
    result.central = false;
    return result;
  }
  result.total = Dot(axis_intersection - moon, away_from_sun) < apex_distance;
  const Vector3 cone_axis = result.total ? away_from_sun * -1.0 : away_from_sun;

  Vector3 helper = std::fabs(cone_axis.z) < 0.9 ? Vector3(0.0, 0.0, 1.0)
                                                : Vector3(0.0, 1.0, 0.0);
  const Vector3 basis_u = Normalize(Cross(cone_axis, helper));
  const Vector3 basis_v = Normalize(Cross(cone_axis, basis_u));
  const double cos_angle = std::cos(half_angle);
  const double sin_angle = std::sin(half_angle);
  result.boundary.reserve(static_cast<std::size_t>(angular_samples));
  for (int sample = 0; sample < angular_samples; ++sample) {
    const double theta = 2.0 * 3.14159265358979323846 * sample /
                         static_cast<double>(angular_samples);
    const Vector3 radial =
        basis_u * std::cos(theta) + basis_v * std::sin(theta);
    const Vector3 ray = Normalize(cone_axis * cos_angle + radial * sin_angle);
    Vector3 intersection;
    if (RayEllipsoidIntersection(apex, ray, ellipsoid,
                                 constants.earth_equatorial_radius_km,
                                 result.total, &intersection)) {
      result.boundary.push_back(EarthFixedToGeodetic(intersection, ellipsoid));
    }
  }
  return result;
}

bool SelectCrossTrackLimits(const ShadowFootprint& footprint,
                            const GeoPoint& axis_before,
                            const GeoPoint& axis_after, GeoPoint* left_limit,
                            GeoPoint* right_limit) {
  if (!left_limit || !right_limit || footprint.boundary.empty()) return false;
  const double latitude = DegreesToRadians(footprint.axis.latitude_deg);
  const double east_velocity =
      NormalizeLongitude(axis_after.longitude_deg - axis_before.longitude_deg) *
      std::cos(latitude);
  const double north_velocity =
      axis_after.latitude_deg - axis_before.latitude_deg;
  const double speed = std::hypot(east_velocity, north_velocity);
  if (speed == 0.0) return false;
  const double normal_east = -north_velocity / speed;
  const double normal_north = east_velocity / speed;
  double maximum = -std::numeric_limits<double>::infinity();
  double minimum = std::numeric_limits<double>::infinity();
  for (std::size_t index = 0; index < footprint.boundary.size(); ++index) {
    const GeoPoint& point = footprint.boundary[index];
    const double east =
        NormalizeLongitude(point.longitude_deg - footprint.axis.longitude_deg) *
        std::cos(latitude);
    const double north = point.latitude_deg - footprint.axis.latitude_deg;
    const double cross_track = east * normal_east + north * normal_north;
    if (cross_track > maximum) {
      maximum = cross_track;
      *left_limit = point;
    }
    if (cross_track < minimum) {
      minimum = cross_track;
      *right_limit = point;
    }
  }
  return true;
}

double SurfaceDistanceKm(const GeoPoint& first, const GeoPoint& second) {
  const double lat1 = DegreesToRadians(first.latitude_deg);
  const double lat2 = DegreesToRadians(second.latitude_deg);
  const double dlat = lat2 - lat1;
  const double dlon = DegreesToRadians(
      NormalizeLongitude(second.longitude_deg - first.longitude_deg));
  const double sine_lat = std::sin(dlat / 2.0);
  const double sine_lon = std::sin(dlon / 2.0);
  const double haversine = sine_lat * sine_lat + std::cos(lat1) *
                                                     std::cos(lat2) * sine_lon *
                                                     sine_lon;
  return 2.0 * 6371.0088 *
         std::asin(std::sqrt(std::max(0.0, std::min(1.0, haversine))));
}

LocalCircumstances EvaluateLocalCircumstances(
    const SolarLunarState& state, const EarthOrientation& orientation,
    const GeoPoint& observer, double height_metres,
    const ReferenceEllipsoid& ellipsoid, const PhysicalConstants& constants) {
  return EvaluateLocalCircumstancesFixed(ToEarthFixed(state, orientation),
                                         observer, height_metres, ellipsoid,
                                         constants);
}

Vector3 ObserverPositionIcrf(const GeoPoint& observer, double height_metres,
                             const EarthOrientation& orientation,
                             const ReferenceEllipsoid& ellipsoid,
                             double earth_equatorial_radius_km) {
  const Vector3 fixed = GeodeticEarthFixed(observer, height_metres, ellipsoid,
                                           earth_equatorial_radius_km);
  const Vector3 x = IcrfToEarthFixed(Vector3(1.0, 0.0, 0.0), orientation);
  const Vector3 y = IcrfToEarthFixed(Vector3(0.0, 1.0, 0.0), orientation);
  const Vector3 z = IcrfToEarthFixed(Vector3(0.0, 0.0, 1.0), orientation);
  return Vector3(Dot(x, fixed), Dot(y, fixed), Dot(z, fixed));
}

EarthFixedSolarLunarState ToEarthFixed(const SolarLunarState& state,
                                       const EarthOrientation& orientation) {
  EarthFixedSolarLunarState fixed;
  fixed.sun_from_earth_km =
      IcrfToEarthFixed(state.sun_from_earth_km, orientation);
  fixed.moon_from_earth_km =
      IcrfToEarthFixed(state.moon_from_earth_km, orientation);
  return fixed;
}

LocalCircumstances EvaluateLocalCircumstancesFixed(
    const EarthFixedSolarLunarState& state, const GeoPoint& observer,
    double height_metres, const ReferenceEllipsoid& ellipsoid,
    const PhysicalConstants& constants) {
  LocalCircumstances result;
  const Vector3 observer_fixed = GeodeticEarthFixed(
      observer, height_metres, ellipsoid, constants.earth_equatorial_radius_km);
  const Vector3 sun_vector = state.sun_from_earth_km - observer_fixed;
  const Vector3 moon_vector = state.moon_from_earth_km - observer_fixed;
  const double sun_distance = sun_vector.Norm();
  const double moon_distance = moon_vector.Norm();
  result.sun_semidiameter_rad =
      std::asin(constants.sun_radius_km / sun_distance);
  result.moon_semidiameter_rad =
      std::asin(constants.moon_radius_km / moon_distance);
  const double cosine =
      std::max(-1.0, std::min(1.0, Dot(sun_vector, moon_vector) /
                                       (sun_distance * moon_distance)));
  result.separation_rad = std::acos(cosine);
  result.partial = result.separation_rad <
                   result.sun_semidiameter_rad + result.moon_semidiameter_rad;
  result.total = result.partial &&
                 result.moon_semidiameter_rad >= result.sun_semidiameter_rad &&
                 result.separation_rad <=
                     result.moon_semidiameter_rad - result.sun_semidiameter_rad;
  result.annular = result.partial &&
                   result.sun_semidiameter_rad > result.moon_semidiameter_rad &&
                   result.separation_rad <= result.sun_semidiameter_rad -
                                                result.moon_semidiameter_rad;
  if (result.total || result.annular) {
    result.magnitude =
        result.moon_semidiameter_rad / result.sun_semidiameter_rad;
  } else {
    result.magnitude =
        std::max(0.0, (result.sun_semidiameter_rad +
                       result.moon_semidiameter_rad - result.separation_rad) /
                          (2.0 * result.sun_semidiameter_rad));
  }
  const double overlap =
      CircleOverlap(result.sun_semidiameter_rad, result.moon_semidiameter_rad,
                    result.separation_rad);
  result.obscuration =
      overlap / (3.14159265358979323846 * result.sun_semidiameter_rad *
                 result.sun_semidiameter_rad);

  const Vector3 sun_fixed = Normalize(sun_vector);
  const double latitude = DegreesToRadians(observer.latitude_deg);
  const double longitude = DegreesToRadians(observer.longitude_deg);
  const Vector3 east(-std::sin(longitude), std::cos(longitude), 0.0);
  const Vector3 north(-std::sin(latitude) * std::cos(longitude),
                      -std::sin(latitude) * std::sin(longitude),
                      std::cos(latitude));
  const Vector3 up(std::cos(latitude) * std::cos(longitude),
                   std::cos(latitude) * std::sin(longitude),
                   std::sin(latitude));
  const double east_component = Dot(sun_fixed, east);
  const double north_component = Dot(sun_fixed, north);
  result.sun_altitude_deg = RadiansToDegrees(
      std::asin(std::max(-1.0, std::min(1.0, Dot(sun_fixed, up)))));
  result.sun_azimuth_deg =
      RadiansToDegrees(std::atan2(east_component, north_component));
  if (result.sun_azimuth_deg < 0.0) result.sun_azimuth_deg += 360.0;
  return result;
}

}  // namespace eclipse
