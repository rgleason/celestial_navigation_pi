#include "LunarDistanceEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <utility>

namespace lunar_distance {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
// Numerical forward-match guard (0.0036 arcsec), not an observational sigma.
// A sign change across a discontinuity is not sufficient to establish a root.
constexpr double kTaggedMatchToleranceDeg = 1e-6;

double ToRadians(double degrees) { return degrees * kPi / 180.0; }
double ToDegrees(double radians) { return radians * 180.0 / kPi; }
double ClampUnit(double value) { return std::max(-1.0, std::min(1.0, value)); }

struct Vector3 {
  double x;
  double y;
  double z;
};

Vector3 operator+(const Vector3& a, const Vector3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vector3 operator*(double scale, const Vector3& value) {
  return {scale * value.x, scale * value.y, scale * value.z};
}
double Dot(const Vector3& a, const Vector3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vector3 Cross(const Vector3& a, const Vector3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double Norm(const Vector3& value) { return std::sqrt(Dot(value, value)); }
Vector3 Unit(const GeographicPoint& point) {
  const double lat = ToRadians(point.latitude_deg);
  const double lon = ToRadians(point.longitude_deg);
  return {std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon),
          std::sin(lat)};
}
GeographicPoint Geographic(const Vector3& value) {
  GeographicPoint point;
  point.latitude_deg =
      ToDegrees(std::atan2(value.z, std::hypot(value.x, value.y)));
  point.longitude_deg = ToDegrees(std::atan2(value.y, value.x));
  return point;
}

double ContactSign(DistanceContact contact) {
  if (contact == DistanceContact::Near) return 1.0;
  if (contact == DistanceContact::Far) return -1.0;
  return 0.0;
}

double AltitudeLimbSign(AltitudeLimb limb) {
  if (limb == AltitudeLimb::Lower) return 1.0;
  if (limb == AltitudeLimb::Upper) return -1.0;
  return 0.0;
}

bool Finite(double value) { return std::isfinite(value); }

// One solve uses a consistent ephemeris snapshot. Cache both success and
// failure by exact epoch, including the covariance probes. There is no shared
// state and no cache carried between calls or provider selections.
class EphemerisCache {
public:
  explicit EphemerisCache(const EphemerisFunction& provider)
      : provider_(provider) {}
  bool Evaluate(double t, EphemerisSample* sample, std::string* error) {
    if (!Finite(t)) {
      if (error) *error = "An ephemeris epoch is not finite";
      return false;
    }
    auto found = entries_.find(t);
    if (found == entries_.end()) {
      Entry entry;
      entry.valid = provider_(t, &entry.sample, &entry.error);
      found = entries_.emplace(t, std::move(entry)).first;
    }
    if (error) *error = found->second.error;
    if (found->second.valid) *sample = found->second.sample;
    return found->second.valid;
  }

private:
  struct Entry {
    bool valid = false;
    EphemerisSample sample;
    std::string error;
  };
  const EphemerisFunction& provider_;
  std::map<double, Entry> entries_;
};

// Retain the historical first-order spherical augmentation, but evaluate it
// consistently at the apparent centre, independently of the selected limb.
double MoonSemidiameter(double nominal, double hp, double apparent_center) {
  return nominal *
         (1.0 + std::sin(ToRadians(apparent_center)) * std::sin(ToRadians(hp)));
}

double ApparentMoonCenter(double limb_altitude, AltitudeLimb limb,
                          double nominal, double hp) {
  double center = limb_altitude;
  for (int i = 0; i < 8; ++i)
    center = limb_altitude +
             AltitudeLimbSign(limb) * MoonSemidiameter(nominal, hp, center);
  return center;
}

// Derivatives are diagnostics, not prerequisites for accepting a root.
// Prefer a central stencil, reducing its size at a geometry/provider boundary.
// If only one side is available, use a bounded one-sided stencil. All steps
// stay inside the caller's search interval when supplied.
template <std::size_t N, class Function>
bool LocalDerivative(const Function& values, double center,
                     const SolveOptions* options, std::array<double, N>* output,
                     bool* one_sided = nullptr) {
  if (one_sided) *one_sided = false;
  std::array<double, N> base{}, before{}, after{}, second{};
  if (!values(center, &base)) return false;
  auto evaluate = [&](double t, std::array<double, N>* result) {
    return (!options || (t >= options->start_offset_seconds &&
                         t <= options->end_offset_seconds)) &&
           values(t, result);
  };
  double saved_step = 0.0;
  int saved_sign = 0;
  std::array<double, N> saved{};
  double step = 30.0;
  for (int iteration = 0; iteration < 13; ++iteration, step *= 0.5) {
    const bool lower = evaluate(center - step, &before);
    const bool upper = evaluate(center + step, &after);
    if (lower && upper) {
      for (std::size_t i = 0; i < N; ++i)
        (*output)[i] = (after[i] - before[i]) / (2.0 * step);
      return std::all_of(output->begin(), output->end(), Finite);
    }
    if ((lower || upper) && !saved_sign) {
      saved_step = step;
      saved_sign = upper ? 1 : -1;
      saved = upper ? after : before;
    }
  }
  if (!saved_sign) return false;
  const bool second_valid =
      evaluate(center + saved_sign * 2.0 * saved_step, &second);
  for (std::size_t i = 0; i < N; ++i)
    (*output)[i] = second_valid
                       ? (-3.0 * base[i] + 4.0 * saved[i] - second[i]) /
                             (2.0 * saved_sign * saved_step)
                       : (saved[i] - base[i]) / (saved_sign * saved_step);
  if (one_sided) *one_sided = true;
  return std::all_of(output->begin(), output->end(), Finite);
}

double RefractionDegrees(double apparent_altitude_deg, double pressure_hpa,
                         double temperature_c, double* refraction_x = nullptr) {
  // Same Bennett/Saemundsson form used by the rest of the plugin. It is not
  // reliable at or below the astronomical horizon.
  const double altitude = ToRadians(apparent_altitude_deg);
  const double denominator = std::tan(altitude) + 0.028;
  if (std::fabs(denominator) < 1e-12)
    return std::numeric_limits<double>::quiet_NaN();
  const double x = std::tan(altitude + ToRadians(0.04848) / denominator);
  if (!Finite(x) || std::fabs(x) < 1e-12 || temperature_c <= -273.15)
    return std::numeric_limits<double>::quiet_NaN();
  if (refraction_x) *refraction_x = x;
  return 0.267 * pressure_hpa / (x * (temperature_c + 273.15)) / 60.0;
}

double ParallaxInAltitude(double horizontal_parallax_deg, double altitude_deg) {
  return ToDegrees(
      std::asin(ClampUnit(std::sin(ToRadians(horizontal_parallax_deg)) *
                          std::cos(ToRadians(altitude_deg)))));
}

bool EvaluateResidual(const Observation& observation,
                      const EphemerisFunction& ephemeris, double offset,
                      double* residual, Clearance* clearance,
                      EphemerisSample* sample, std::string* error) {
  EphemerisSample local_sample;
  if (!ephemeris(offset, &local_sample, error)) return false;
  Clearance local_clearance = ClearDistance(observation, local_sample);
  if (!local_clearance.valid) {
    if (error) *error = local_clearance.error;
    return false;
  }
  if (residual)
    *residual = local_sample.predicted_distance_deg -
                local_clearance.cleared_distance_deg;
  if (clearance) *clearance = local_clearance;
  if (sample) *sample = local_sample;
  return true;
}

double AngularUncertainty(const Observation& observation,
                          const EphemerisSample& sample,
                          const Clearance& nominal,
                          double contributions_arcmin[3] = nullptr) {
  const double inputs[3] = {observation.distance_uncertainty_arcmin,
                            observation.moon_altitude_uncertainty_arcmin,
                            observation.body_altitude_uncertainty_arcmin};
  double variance = 0.0;
  for (int index = 0; index < 3; ++index) {
    if (contributions_arcmin) contributions_arcmin[index] = 0.0;
    if (!(inputs[index] > 0.0)) continue;
    Observation perturbed = observation;
    // Estimate the local derivative with a small angular step, then scale it
    // by the user's uncertainty.  Perturbing by the full sigma can cross a
    // spherical-geometry boundary and incorrectly discard that contribution.
    const double step_deg = 1e-5;
    if (index == 0)
      perturbed.raw_distance_deg += step_deg;
    else if (index == 1)
      perturbed.moon_altitude_deg += step_deg;
    else
      perturbed.body_altitude_deg += step_deg;
    const Clearance changed = ClearDistance(perturbed, sample);
    if (!changed.valid) continue;
    const double sensitivity =
        (changed.cleared_distance_deg - nominal.cleared_distance_deg) /
        step_deg;
    const double contribution = std::fabs(sensitivity) * inputs[index];
    if (contributions_arcmin) contributions_arcmin[index] = contribution;
    variance += contribution * contribution;
  }
  return std::sqrt(variance);
}

double DipDegrees(const Observation& observation) {
  if (observation.artificial_horizon) return 0.0;
  return observation.dip_short
             ? (0.4156 * observation.dip_short_distance_m +
                1.856 * observation.eye_height_m /
                    observation.dip_short_distance_m) /
                   60.0
             : 1.758 * std::sqrt(observation.eye_height_m) / 60.0;
}

bool CorrectObservedAltitude(const Observation& observation,
                             const EphemerisSample& sample, bool moon,
                             double* geocentric_altitude_deg,
                             std::string* error) {
  const double raw =
      moon ? observation.moon_altitude_deg : observation.body_altitude_deg;
  const AltitudeLimb limb =
      moon ? observation.moon_altitude_limb : observation.body_altitude_limb;
  const double hp = moon ? sample.moon_horizontal_parallax_deg
                         : sample.body_horizontal_parallax_deg;
  const double nominal_sd =
      moon ? sample.moon_semidiameter_deg : sample.body_semidiameter_deg;
  if (!Finite(raw) || !Finite(hp) || !Finite(nominal_sd)) {
    if (error) *error = "An altitude correction input is not finite";
    return false;
  }
  double limb_altitude =
      raw - observation.index_error_arcmin / 60.0 - DipDegrees(observation);
  if (observation.artificial_horizon) limb_altitude *= 0.5;
  const double apparent_center =
      moon ? ApparentMoonCenter(limb_altitude, limb, nominal_sd, hp)
           : limb_altitude + AltitudeLimbSign(limb) * nominal_sd;
  const double refraction = RefractionDegrees(
      apparent_center, observation.pressure_hpa, observation.temperature_c);
  if (!Finite(refraction) || apparent_center <= -1.0 ||
      apparent_center >= 90.0) {
    if (error) *error = "An altitude is outside the usable refraction range";
    return false;
  }
  const double topocentric = apparent_center - refraction;
  *geocentric_altitude_deg = topocentric + ParallaxInAltitude(hp, topocentric);
  return Finite(*geocentric_altitude_deg);
}

double NormalizeLongitude(double longitude) {
  longitude = std::fmod(longitude + 180.0, 360.0);
  if (longitude < 0.0) longitude += 360.0;
  return longitude - 180.0;
}

GeographicPoint Destination(const GeographicPoint& start, double bearing_deg,
                            double distance_nm) {
  const double angular = ToRadians(distance_nm / 60.0);
  const double bearing = ToRadians(bearing_deg);
  const double latitude = ToRadians(start.latitude_deg);
  const double longitude = ToRadians(start.longitude_deg);
  const double destination_latitude = std::asin(
      ClampUnit(std::sin(latitude) * std::cos(angular) +
                std::cos(latitude) * std::sin(angular) * std::cos(bearing)));
  const double destination_longitude =
      longitude +
      std::atan2(std::sin(bearing) * std::sin(angular) * std::cos(latitude),
                 std::cos(angular) -
                     std::sin(latitude) * std::sin(destination_latitude));
  return {ToDegrees(destination_latitude),
          NormalizeLongitude(ToDegrees(destination_longitude))};
}

GeographicPoint ObserverAt(const GeographicPoint& reference,
                           const Observation& observation,
                           double relative_seconds) {
  if (!observation.moving_observer || observation.speed_knots == 0.0 ||
      relative_seconds == 0.0)
    return reference;
  double bearing = observation.course_true_deg;
  double distance = observation.speed_knots * relative_seconds / 3600.0;
  if (distance < 0.0) {
    distance = -distance;
    bearing += 180.0;
  }
  if (observation.use_ellipsoid) {
    // Vincenty's direct geodesic, using only local variables (the legacy
    // geodesic.c helper has shared mutable state and is unsafe in workers).
    constexpr double a = 6378137.0, f = 1.0 / 298.257223563;
    constexpr double b = a * (1.0 - f);
    const double u =
        std::atan((1.0 - f) * std::tan(ToRadians(reference.latitude_deg)));
    const double az = ToRadians(bearing);
    const double sa = std::cos(u) * std::sin(az);
    const double ca2 = 1.0 - sa * sa;
    const double u2 = ca2 * (a * a - b * b) / (b * b);
    const double A =
        1.0 +
        u2 / 16384.0 * (4096.0 + u2 * (-768.0 + u2 * (320.0 - 175.0 * u2)));
    const double B =
        u2 / 1024.0 * (256.0 + u2 * (-128.0 + u2 * (74.0 - 47.0 * u2)));
    const double sigma1 = std::atan2(std::tan(u), std::cos(az));
    const double base = distance * 1852.0 / (b * A);
    double sigma = base;
    for (int i = 0; i < 20; ++i) {
      const double c = std::cos(2.0 * sigma1 + sigma);
      const double s = std::sin(sigma), cs = std::cos(sigma);
      const double next =
          base + B * s *
                     (c + B / 4.0 *
                              (cs * (-1.0 + 2.0 * c * c) -
                               B / 6.0 * c * (-3.0 + 4.0 * s * s) *
                                   (-3.0 + 4.0 * c * c)));
      if (std::fabs(next - sigma) < 1e-13) {
        sigma = next;
        break;
      }
      sigma = next;
    }
    const double s = std::sin(sigma), cs = std::cos(sigma);
    const double t = std::sin(u) * s - std::cos(u) * cs * std::cos(az);
    const double latitude =
        std::atan2(std::sin(u) * cs + std::cos(u) * s * std::cos(az),
                   (1.0 - f) * std::hypot(sa, t));
    const double lambda = std::atan2(
        s * std::sin(az), std::cos(u) * cs - std::sin(u) * s * std::cos(az));
    const double C = f / 16.0 * ca2 * (4.0 + f * (4.0 - 3.0 * ca2));
    const double c = std::cos(2.0 * sigma1 + sigma);
    const double delta_lon =
        lambda - (1.0 - C) * f * sa *
                     (sigma + C * s * (c + C * cs * (-1.0 + 2.0 * c * c)));
    return {ToDegrees(latitude),
            NormalizeLongitude(reference.longitude_deg + ToDegrees(delta_lon))};
  }
  return Destination(reference, bearing, distance);
}

// GP directions are geocentric; the observer latitude and local vertical are
// geodetic. Do not convert the GP latitude to geodetic latitude.
void EllipsoidalDirection(const Observation& observation,
                          const EphemerisSample& sample,
                          const GeographicPoint& observer, bool moon,
                          double* altitude, double* azimuth, double* sd) {
  if (sample.observer_direction) {
    if (!sample.observer_direction(
            observer.latitude_deg, observer.longitude_deg,
            observation.eye_height_m, moon, altitude, azimuth, sd))
      *altitude = *azimuth = *sd = std::numeric_limits<double>::quiet_NaN();
    return;
  }
  constexpr double a = 6378.137;  // km; HP uses the equatorial radius
  constexpr double f = 1.0 / 298.257223563;
  constexpr double e2 = f * (2.0 - f);
  const double latitude = ToRadians(observer.latitude_deg);
  const double longitude = ToRadians(observer.longitude_deg);
  const double n =
      a / std::sqrt(1.0 - e2 * std::sin(latitude) * std::sin(latitude));
  // Sea-level horizon observation: eye height approximates ellipsoidal height.
  const double h = observation.eye_height_m / 1000.0;
  const Vector3 station{(n + h) * std::cos(latitude) * std::cos(longitude),
                        (n + h) * std::cos(latitude) * std::sin(longitude),
                        (n * (1.0 - e2) + h) * std::sin(latitude)};
  const Vector3 direction =
      Unit(moon ? GeographicPoint(sample.moon_geographic_latitude_deg,
                                  sample.moon_geographic_longitude_deg)
                : GeographicPoint(sample.body_geographic_latitude_deg,
                                  sample.body_geographic_longitude_deg));
  const double hp = moon ? sample.moon_horizontal_parallax_deg
                         : sample.body_horizontal_parallax_deg;
  *sd = moon ? sample.moon_semidiameter_deg : sample.body_semidiameter_deg;
  Vector3 topocentric = direction;
  if (hp > 0.0) {
    const double range = a / std::sin(ToRadians(hp));
    topocentric = range * direction + (-1.0) * station;
    const double topocentric_range = Norm(topocentric);
    *sd = ToDegrees(std::asin(
        ClampUnit(range * std::sin(ToRadians(*sd)) / topocentric_range)));
    topocentric = (1.0 / topocentric_range) * topocentric;
  }
  const Vector3 up = Unit(observer);
  const Vector3 east{-std::sin(longitude), std::cos(longitude), 0.0};
  const Vector3 north = Cross(up, east);
  *altitude = ToDegrees(std::asin(ClampUnit(Dot(topocentric, up))));
  *azimuth =
      ToDegrees(std::atan2(Dot(topocentric, east), Dot(topocentric, north)));
}

void AltitudeAzimuth(const GeographicPoint& observer,
                     const GeographicPoint& geographic_position,
                     double* altitude_deg, double* azimuth_deg) {
  const double latitude = ToRadians(observer.latitude_deg);
  const double declination = ToRadians(geographic_position.latitude_deg);
  const double longitude_difference =
      ToRadians(geographic_position.longitude_deg - observer.longitude_deg);
  const double sine_altitude = std::sin(latitude) * std::sin(declination) +
                               std::cos(latitude) * std::cos(declination) *
                                   std::cos(longitude_difference);
  *altitude_deg = ToDegrees(std::asin(ClampUnit(sine_altitude)));
  const double y = std::sin(longitude_difference) * std::cos(declination);
  const double x = std::cos(latitude) * std::sin(declination) -
                   std::sin(latitude) * std::cos(declination) *
                       std::cos(longitude_difference);
  *azimuth_deg = ToDegrees(std::atan2(y, x));
}

double TopocentricFromGeocentric(double geocentric_altitude_deg,
                                 double horizontal_parallax_deg) {
  double topocentric = geocentric_altitude_deg;
  for (int iteration = 0; iteration < 8; ++iteration)
    topocentric = geocentric_altitude_deg -
                  ParallaxInAltitude(horizontal_parallax_deg, topocentric);
  return topocentric;
}

double ApparentFromTopocentric(double topocentric_altitude_deg,
                               const Observation& observation) {
  double apparent = topocentric_altitude_deg;
  for (int iteration = 0; iteration < 8; ++iteration) {
    const double refraction = RefractionDegrees(
        apparent, observation.pressure_hpa, observation.temperature_c);
    if (!Finite(refraction)) return std::numeric_limits<double>::quiet_NaN();
    apparent = topocentric_altitude_deg + refraction;
  }
  return apparent;
}

double PredictedRawAltitude(const Observation& observation,
                            const EphemerisSample& sample,
                            const GeographicPoint& observer, bool moon) {
  if (observation.use_ellipsoid) {
    double altitude, azimuth, sd;
    EllipsoidalDirection(observation, sample, observer, moon, &altitude,
                         &azimuth, &sd);
    const auto limb =
        moon ? observation.moon_altitude_limb : observation.body_altitude_limb;
    const double apparent_limb = ApparentFromTopocentric(
        altitude - AltitudeLimbSign(limb) * sd, observation);
    return (observation.artificial_horizon ? 2.0 : 1.0) * apparent_limb +
           observation.index_error_arcmin / 60.0 + DipDegrees(observation);
  }
  const GeographicPoint gp =
      moon ? GeographicPoint(sample.moon_geographic_latitude_deg,
                             sample.moon_geographic_longitude_deg)
           : GeographicPoint(sample.body_geographic_latitude_deg,
                             sample.body_geographic_longitude_deg);
  double geocentric = 0.0, azimuth = 0.0;
  AltitudeAzimuth(observer, gp, &geocentric, &azimuth);
  const double hp = moon ? sample.moon_horizontal_parallax_deg
                         : sample.body_horizontal_parallax_deg;
  const double nominal_sd =
      moon ? sample.moon_semidiameter_deg : sample.body_semidiameter_deg;
  const AltitudeLimb limb =
      moon ? observation.moon_altitude_limb : observation.body_altitude_limb;
  const double topocentric = TopocentricFromGeocentric(geocentric, hp);
  const double apparent_center =
      ApparentFromTopocentric(topocentric, observation);
  if (!Finite(apparent_center)) return std::numeric_limits<double>::quiet_NaN();
  const double sd =
      moon ? MoonSemidiameter(nominal_sd, hp, apparent_center) : nominal_sd;
  const double limb_altitude = apparent_center - AltitudeLimbSign(limb) * sd;
  const double corrected =
      observation.artificial_horizon ? 2.0 * limb_altitude : limb_altitude;
  return corrected + observation.index_error_arcmin / 60.0 +
         DipDegrees(observation);
}

double PredictedRawDistance(const Observation& observation,
                            const EphemerisSample& sample,
                            const GeographicPoint& observer) {
  if (observation.use_ellipsoid) {
    double moon_alt, moon_az, moon_sd, body_alt, body_az, body_sd;
    EllipsoidalDirection(observation, sample, observer, true, &moon_alt,
                         &moon_az, &moon_sd);
    EllipsoidalDirection(observation, sample, observer, false, &body_alt,
                         &body_az, &body_sd);
    const double ma = ToRadians(ApparentFromTopocentric(moon_alt, observation));
    const double ba = ToRadians(ApparentFromTopocentric(body_alt, observation));
    const double az = ToRadians(moon_az - body_az);
    const double separation =
        std::acos(ClampUnit(std::sin(ma) * std::sin(ba) +
                            std::cos(ma) * std::cos(ba) * std::cos(az)));
    // Differential refraction flattens each apparent disc vertically. Project
    // the refracted semidiameter in the direction of the other body's centre.
    auto contact_radius = [&](double alt, double other, double sd) {
      const double apparent = ApparentFromTopocentric(alt, observation);
      const double vertical = (ApparentFromTopocentric(alt + sd, observation) -
                               ApparentFromTopocentric(alt - sd, observation)) /
                              2.0;
      const double cosine =
          std::fabs(std::sin(separation)) < 1e-12
              ? 0.0
              : ClampUnit(
                    (std::sin(other) -
                     std::sin(ToRadians(apparent)) * std::cos(separation)) /
                    (std::cos(ToRadians(apparent)) * std::sin(separation)));
      return std::hypot(vertical * cosine,
                        sd * std::sqrt(std::max(0.0, 1.0 - cosine * cosine)));
    };
    return ToDegrees(separation) + observation.index_error_arcmin / 60.0 -
           ContactSign(observation.moon_contact) *
               contact_radius(moon_alt, ba, moon_sd) -
           ContactSign(observation.body_contact) *
               contact_radius(body_alt, ma, body_sd);
  }
  double moon_geocentric = 0.0, moon_azimuth = 0.0;
  double body_geocentric = 0.0, body_azimuth = 0.0;
  AltitudeAzimuth(observer,
                  {sample.moon_geographic_latitude_deg,
                   sample.moon_geographic_longitude_deg},
                  &moon_geocentric, &moon_azimuth);
  AltitudeAzimuth(observer,
                  {sample.body_geographic_latitude_deg,
                   sample.body_geographic_longitude_deg},
                  &body_geocentric, &body_azimuth);
  const double moon_apparent = ApparentFromTopocentric(
      TopocentricFromGeocentric(moon_geocentric,
                                sample.moon_horizontal_parallax_deg),
      observation);
  const double body_apparent = ApparentFromTopocentric(
      TopocentricFromGeocentric(body_geocentric,
                                sample.body_horizontal_parallax_deg),
      observation);
  if (!Finite(moon_apparent) || !Finite(body_apparent))
    return std::numeric_limits<double>::quiet_NaN();
  const double azimuth_difference = ToRadians(moon_azimuth - body_azimuth);
  const double apparent_distance = ToDegrees(std::acos(ClampUnit(
      std::sin(ToRadians(moon_apparent)) * std::sin(ToRadians(body_apparent)) +
      std::cos(ToRadians(moon_apparent)) * std::cos(ToRadians(body_apparent)) *
          std::cos(azimuth_difference))));
  const double moon_sd =
      MoonSemidiameter(sample.moon_semidiameter_deg,
                       sample.moon_horizontal_parallax_deg, moon_apparent);
  return apparent_distance + observation.index_error_arcmin / 60.0 -
         ContactSign(observation.moon_contact) * moon_sd -
         ContactSign(observation.body_contact) * sample.body_semidiameter_deg;
}

double CalculatedGeocentricAltitude(const GeographicPoint& observer,
                                    const GeographicPoint& gp) {
  double altitude = 0.0, azimuth = 0.0;
  AltitudeAzimuth(observer, gp, &altitude, &azimuth);
  return altitude;
}

std::vector<GeographicPoint> SolveReferencePositions(
    const Observation& observation, const EphemerisSample& moon_sample,
    const EphemerisSample& body_sample, double moon_altitude,
    double body_altitude, std::string* error, std::vector<int>* branch_ids) {
  const GeographicPoint moon_gp{moon_sample.moon_geographic_latitude_deg,
                                moon_sample.moon_geographic_longitude_deg};
  const GeographicPoint body_gp{body_sample.body_geographic_latitude_deg,
                                body_sample.body_geographic_longitude_deg};
  PositionResult seeds =
      IntersectAltitudeCircles(moon_gp, moon_altitude, body_gp, body_altitude);
  if (!seeds.valid && observation.use_ellipsoid &&
      Finite(moon_gp.latitude_deg) && Finite(moon_gp.longitude_deg) &&
      Finite(body_gp.latitude_deg) && Finite(body_gp.longitude_deg) &&
      std::fabs(moon_gp.latitude_deg) <= 90.0 &&
      std::fabs(body_gp.latitude_deg) <= 90.0) {
    // Spherical non-intersection is not proof of WGS84 non-intersection.
    // Near tangency, geodetic vertical/parallax corrections can move the real
    // intersections beyond the spherical seed boundary. Start on either side
    // of the closest spherical point and let the full forward constraints,
    // not the approximate seed geometry, determine convergence.
    const Vector3 moon = Unit(moon_gp), body = Unit(body_gp);
    const double dot = ClampUnit(Dot(moon, body));
    const double denominator = 1.0 - dot * dot;
    if (denominator >= 1e-12) {
      const double m = std::sin(ToRadians(moon_altitude));
      const double b = std::sin(ToRadians(body_altitude));
      const Vector3 base = ((m - dot * b) / denominator) * moon +
                           ((b - dot * m) / denominator) * body;
      const double length = Norm(base);
      // This bound limits how far from spherical feasibility we attempt a
      // rescue; it is a seed/work bound, never a forward acceptance tolerance.
      if (Finite(length) && length > 1.0 && length < 1.02) {
        const Vector3 center = (1.0 / length) * base;
        const Vector3 cross = Cross(moon, body);
        const Vector3 normal = (1.0 / Norm(cross)) * cross;
        const double spread = ToRadians(1.0);
        seeds.candidates.push_back(
            Geographic(std::cos(spread) * center + std::sin(spread) * normal));
        seeds.candidates.push_back(Geographic(std::cos(spread) * center +
                                              (-std::sin(spread)) * normal));
        seeds.valid = true;
      }
    }
  }
  if (!seeds.valid) {
    if (error) *error = seeds.error;
    return {};
  }
  if (!observation.use_ellipsoid &&
      (!observation.moving_observer || observation.speed_knots == 0.0)) {
    for (std::size_t i = 0; i < seeds.candidates.size(); ++i)
      branch_ids->push_back(static_cast<int>(i));
    return seeds.candidates;
  }

  auto residual = [&](const GeographicPoint& observer, bool moon) {
    if (observation.use_ellipsoid)
      return PredictedRawAltitude(observation, moon ? moon_sample : body_sample,
                                  observer, moon) -
             (moon ? observation.moon_altitude_deg
                   : observation.body_altitude_deg);
    return CalculatedGeocentricAltitude(observer, moon ? moon_gp : body_gp) -
           (moon ? moon_altitude : body_altitude);
  };

  std::vector<GeographicPoint> result;
  for (std::size_t seed = 0; seed < seeds.candidates.size(); ++seed) {
    GeographicPoint position = seeds.candidates[seed];
    bool converged = false;
    for (int iteration = 0; iteration < 30; ++iteration) {
      const GeographicPoint moon_observer = ObserverAt(
          position, observation, observation.moon_time_offset_seconds);
      const GeographicPoint body_observer = ObserverAt(
          position, observation, observation.body_time_offset_seconds);
      const double f0 = residual(moon_observer, true);
      const double f1 = residual(body_observer, false);
      if (!Finite(f0) || !Finite(f1)) break;
      if (std::hypot(f0, f1) < 1e-9) {
        converged = true;
        break;
      }
      const double step = 1e-5;
      GeographicPoint latitude_step = position;
      latitude_step.latitude_deg += step;
      GeographicPoint longitude_step = position;
      longitude_step.longitude_deg += step;
      const GeographicPoint moon_lat_observer = ObserverAt(
          latitude_step, observation, observation.moon_time_offset_seconds);
      const GeographicPoint body_lat_observer = ObserverAt(
          latitude_step, observation, observation.body_time_offset_seconds);
      const GeographicPoint moon_lon_observer = ObserverAt(
          longitude_step, observation, observation.moon_time_offset_seconds);
      const GeographicPoint body_lon_observer = ObserverAt(
          longitude_step, observation, observation.body_time_offset_seconds);
      const double j00 = (residual(moon_lat_observer, true) - f0) / step;
      const double j10 = (residual(body_lat_observer, false) - f1) / step;
      const double j01 = (residual(moon_lon_observer, true) - f0) / step;
      const double j11 = (residual(body_lon_observer, false) - f1) / step;
      const double determinant = j00 * j11 - j01 * j10;
      if (!Finite(determinant) || std::fabs(determinant) < 1e-12) break;
      double dlat = (-f0 * j11 + j01 * f1) / determinant;
      double dlon = (-j00 * f1 + f0 * j10) / determinant;
      const double damping =
          std::max(1.0, std::max(std::fabs(dlat), std::fabs(dlon)) / 2.0);
      position.latitude_deg += dlat / damping;
      position.longitude_deg =
          NormalizeLongitude(position.longitude_deg + dlon / damping);
      if (position.latitude_deg <= -89.999 || position.latitude_deg >= 89.999)
        break;
    }
    if (!converged) continue;
    bool duplicate = false;
    for (const GeographicPoint& existing : result)
      if (ToDegrees(std::acos(ClampUnit(Dot(Unit(existing), Unit(position))))) <
          1e-6)
        duplicate = true;
    if (!duplicate) {
      result.push_back(position);
      branch_ids->push_back(static_cast<int>(seed));
    }
  }
  if (result.empty() && error)
    *error = "The observer altitude constraints did not converge";
  return result;
}

struct TaggedEvaluation {
  bool valid = false;
  std::string error;
  EphemerisSample distance_sample;
  EphemerisSample moon_sample;
  EphemerisSample body_sample;
  std::vector<GeographicPoint> positions;
  std::vector<PositionGeometry> geometry;
  std::vector<double> residuals;
  std::vector<int> branch_ids;
};

int BranchIndex(const TaggedEvaluation& evaluation, int id) {
  if (!evaluation.valid) return -1;
  const auto it =
      std::find(evaluation.branch_ids.begin(), evaluation.branch_ids.end(), id);
  return it == evaluation.branch_ids.end()
             ? -1
             : static_cast<int>(it - evaluation.branch_ids.begin());
}

TaggedEvaluation EvaluateTagged(const Observation& observation,
                                const EphemerisFunction& ephemeris,
                                double correction_seconds) {
  TaggedEvaluation result;
  if (!ephemeris(correction_seconds, &result.distance_sample, &result.error) ||
      !ephemeris(correction_seconds + observation.moon_time_offset_seconds,
                 &result.moon_sample, &result.error) ||
      !ephemeris(correction_seconds + observation.body_time_offset_seconds,
                 &result.body_sample, &result.error))
    return result;
  double moon_altitude = 0.0, body_altitude = 0.0;
  if (!CorrectObservedAltitude(observation, result.moon_sample, true,
                               &moon_altitude, &result.error) ||
      !CorrectObservedAltitude(observation, result.body_sample, false,
                               &body_altitude, &result.error))
    return result;
  result.positions = SolveReferencePositions(
      observation, result.moon_sample, result.body_sample, moon_altitude,
      body_altitude, &result.error, &result.branch_ids);
  for (const GeographicPoint& position : result.positions) {
    result.geometry.push_back(CalculatePositionGeometry(
        position,
        {result.moon_sample.moon_geographic_latitude_deg,
         result.moon_sample.moon_geographic_longitude_deg},
        {result.body_sample.body_geographic_latitude_deg,
         result.body_sample.body_geographic_longitude_deg}));
    const double predicted =
        PredictedRawDistance(observation, result.distance_sample, position);
    if (!Finite(predicted)) {
      result.error = "The forward lunar distance is not finite";
      return result;
    }
    result.residuals.push_back(predicted - observation.raw_distance_deg);
  }
  result.valid = !result.positions.empty() &&
                 result.positions.size() == result.residuals.size();
  if (!result.valid && result.error.empty())
    result.error = "No time-tagged lunar position could be evaluated";
  return result;
}

struct TaggedUncertainty {
  bool valid = false;
  double time_seconds = std::numeric_limits<double>::infinity();
  double position_nm = std::numeric_limits<double>::infinity();
};

bool InvertThreeByThree(const double input[3][3], double inverse[3][3]) {
  double augmented[3][6] = {};
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column)
      augmented[row][column] = input[row][column];
    augmented[row][row + 3] = 1.0;
  }
  for (int pivot = 0; pivot < 3; ++pivot) {
    int best = pivot;
    for (int row = pivot + 1; row < 3; ++row)
      if (std::fabs(augmented[row][pivot]) > std::fabs(augmented[best][pivot]))
        best = row;
    if (std::fabs(augmented[best][pivot]) < 1e-12) return false;
    if (best != pivot)
      for (int column = 0; column < 6; ++column)
        std::swap(augmented[pivot][column], augmented[best][column]);
    const double divisor = augmented[pivot][pivot];
    for (int column = 0; column < 6; ++column)
      augmented[pivot][column] /= divisor;
    for (int row = 0; row < 3; ++row) {
      if (row == pivot) continue;
      const double factor = augmented[row][pivot];
      for (int column = 0; column < 6; ++column)
        augmented[row][column] -= factor * augmented[pivot][column];
    }
  }
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 3; ++column)
      inverse[row][column] = augmented[row][column + 3];
  return true;
}

TaggedUncertainty EstimateTaggedUncertainty(
    const Observation& observation, const EphemerisFunction& ephemeris,
    double correction_seconds, const GeographicPoint& reference_position,
    const SolveOptions* options = nullptr) {
  TaggedUncertainty result;
  constexpr double coordinate_step_deg = 1e-4;
  double jacobian[3][3] = {};

  auto values = [&](double correction, GeographicPoint position,
                    double output[3]) {
    const PredictedObservation predicted = PredictTimeTaggedObservation(
        observation, ephemeris, correction, position);
    if (!predicted.valid) return false;
    output[0] = predicted.raw_distance_deg;
    output[1] = predicted.moon_altitude_deg;
    output[2] = predicted.body_altitude_deg;
    return true;
  };

  auto time_values = [&](double t, std::array<double, 3>* out) {
    return values(t, reference_position, out->data());
  };
  std::array<double, 3> time_derivative{};
  if (!LocalDerivative(time_values, correction_seconds, options,
                       &time_derivative))
    return result;
  for (int i = 0; i < 3; ++i) jacobian[i][0] = time_derivative[i] * 3600.0;
  double before[3], after[3];

  for (int coordinate = 0; coordinate < 2; ++coordinate) {
    GeographicPoint lower = reference_position;
    GeographicPoint upper = reference_position;
    if (coordinate == 0) {
      lower.latitude_deg -= coordinate_step_deg;
      upper.latitude_deg += coordinate_step_deg;
    } else {
      lower.longitude_deg -= coordinate_step_deg;
      upper.longitude_deg += coordinate_step_deg;
    }
    if (!values(correction_seconds, lower, before) ||
        !values(correction_seconds, upper, after))
      return result;
    for (int observation_index = 0; observation_index < 3; ++observation_index)
      jacobian[observation_index][coordinate + 1] =
          (after[observation_index] - before[observation_index]) /
          (2.0 * coordinate_step_deg);
  }

  const double sigma_deg[3] = {
      std::max(1e-6, observation.distance_uncertainty_arcmin / 60.0),
      std::max(1e-6, observation.moon_altitude_uncertainty_arcmin / 60.0),
      std::max(1e-6, observation.body_altitude_uncertainty_arcmin / 60.0)};
  double normal[3][3] = {};
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 3; ++column)
      for (int observation_index = 0; observation_index < 3;
           ++observation_index)
        normal[row][column] +=
            jacobian[observation_index][row] *
            jacobian[observation_index][column] /
            (sigma_deg[observation_index] * sigma_deg[observation_index]);
  double covariance[3][3] = {};
  if (!InvertThreeByThree(normal, covariance) || covariance[0][0] < 0.0 ||
      covariance[1][1] < 0.0 || covariance[2][2] < 0.0)
    return result;
  result.time_seconds = 3600.0 * std::sqrt(covariance[0][0]);
  const double cos_latitude =
      std::cos(ToRadians(reference_position.latitude_deg));
  result.position_nm =
      60.0 * std::sqrt(covariance[1][1] +
                       cos_latitude * cos_latitude * covariance[2][2]);
  result.valid = Finite(result.time_seconds) && Finite(result.position_nm);
  return result;
}

}  // namespace

PositionGeometry CalculatePositionGeometry(
    const GeographicPoint& observer,
    const GeographicPoint& moon_geographic_position,
    const GeographicPoint& body_geographic_position) {
  double moon_altitude = 0.0;
  double body_altitude = 0.0;
  PositionGeometry result;
  AltitudeAzimuth(observer, moon_geographic_position, &moon_altitude,
                  &result.moon_azimuth_deg);
  AltitudeAzimuth(observer, body_geographic_position, &body_altitude,
                  &result.body_azimuth_deg);
  auto normalize = [](double value) {
    value = std::fmod(value, 360.0);
    return value < 0.0 ? value + 360.0 : value;
  };
  result.moon_azimuth_deg = normalize(result.moon_azimuth_deg);
  result.body_azimuth_deg = normalize(result.body_azimuth_deg);
  double separation =
      std::fabs(result.moon_azimuth_deg - result.body_azimuth_deg);
  if (separation > 180.0) separation = 360.0 - separation;
  result.azimuth_separation_deg = separation;
  result.effective_crossing_angle_deg =
      std::min(separation, 180.0 - separation);
  return result;
}

GeographicPoint AdvanceObserver(const GeographicPoint& reference,
                                const Observation& observation,
                                double relative_seconds) {
  return ObserverAt(reference, observation, relative_seconds);
}

Clearance ClearDistance(const Observation& observation,
                        const EphemerisSample& ephemeris) {
  Clearance result;
  const double values[] = {
      observation.raw_distance_deg,    observation.moon_altitude_deg,
      observation.body_altitude_deg,   observation.index_error_arcmin,
      observation.eye_height_m,        observation.pressure_hpa,
      observation.temperature_c,       ephemeris.predicted_distance_deg,
      ephemeris.moon_semidiameter_deg, ephemeris.moon_horizontal_parallax_deg,
      ephemeris.body_semidiameter_deg, ephemeris.body_horizontal_parallax_deg};
  for (double value : values) {
    if (!Finite(value)) {
      result.error = "A lunar-distance input is not finite";
      return result;
    }
  }
  if (observation.raw_distance_deg <= 0.0 ||
      observation.raw_distance_deg >= 180.0) {
    result.error =
        "The measured lunar distance must be between 0 and 180 degrees";
    return result;
  }
  if (observation.eye_height_m < 0.0 || observation.pressure_hpa <= 0.0 ||
      observation.temperature_c <= -100.0) {
    result.error =
        "Eye height, pressure or temperature is outside a usable range";
    return result;
  }
  if (observation.dip_short && observation.dip_short_distance_m <= 0.0) {
    result.error = "Dip-short distance must be greater than zero";
    return result;
  }

  const double index_correction_deg = observation.index_error_arcmin / 60.0;
  result.index_correction_deg = index_correction_deg;
  double dip_deg = 0.0;
  if (!observation.artificial_horizon) {
    dip_deg = observation.dip_short
                  ? (0.4156 * observation.dip_short_distance_m +
                     1.856 * observation.eye_height_m /
                         observation.dip_short_distance_m) /
                        60.0
                  : 1.758 * std::sqrt(observation.eye_height_m) / 60.0;
  }
  result.dip_correction_deg = dip_deg;

  auto apparent_limb_altitude = [&](double hs) {
    double value = hs - index_correction_deg - dip_deg;
    if (observation.artificial_horizon) value *= 0.5;
    return value;
  };

  const double moon_limb_alt =
      apparent_limb_altitude(observation.moon_altitude_deg);
  const double body_limb_alt =
      apparent_limb_altitude(observation.body_altitude_deg);
  result.moon_apparent_limb_altitude_deg = moon_limb_alt;
  result.body_apparent_limb_altitude_deg = body_limb_alt;
  result.moon_semidiameter_deg = ephemeris.moon_semidiameter_deg;
  result.moon_horizontal_parallax_deg = ephemeris.moon_horizontal_parallax_deg;
  result.body_horizontal_parallax_deg = ephemeris.body_horizontal_parallax_deg;
  const double moon_center = ApparentMoonCenter(
      moon_limb_alt, observation.moon_altitude_limb,
      ephemeris.moon_semidiameter_deg, ephemeris.moon_horizontal_parallax_deg);
  const double moon_topocentric_sd =
      MoonSemidiameter(ephemeris.moon_semidiameter_deg,
                       ephemeris.moon_horizontal_parallax_deg, moon_center);
  result.moon_topocentric_semidiameter_deg = moon_topocentric_sd;
  result.body_semidiameter_deg = ephemeris.body_semidiameter_deg;
  result.moon_altitude_limb_correction_deg =
      AltitudeLimbSign(observation.moon_altitude_limb) * moon_topocentric_sd;
  result.body_altitude_limb_correction_deg =
      AltitudeLimbSign(observation.body_altitude_limb) *
      ephemeris.body_semidiameter_deg;
  result.moon_apparent_center_altitude_deg =
      moon_limb_alt + result.moon_altitude_limb_correction_deg;
  result.body_apparent_center_altitude_deg =
      body_limb_alt + result.body_altitude_limb_correction_deg;

  if (result.moon_apparent_center_altitude_deg <= -1.0 ||
      result.body_apparent_center_altitude_deg <= -1.0 ||
      result.moon_apparent_center_altitude_deg >= 90.0 ||
      result.body_apparent_center_altitude_deg >= 90.0) {
    result.error =
        "Both body centres must be above the usable astronomical horizon";
    return result;
  }

  result.moon_distance_limb_correction_deg =
      ContactSign(observation.moon_contact) * moon_topocentric_sd;
  result.body_distance_limb_correction_deg =
      ContactSign(observation.body_contact) * ephemeris.body_semidiameter_deg;
  result.apparent_distance_deg = observation.raw_distance_deg -
                                 index_correction_deg +
                                 result.moon_distance_limb_correction_deg +
                                 result.body_distance_limb_correction_deg;
  if (observation.artificial_horizon) {
    // The artificial horizon doubles altitudes, not the inter-body distance.
  }
  if (result.apparent_distance_deg <= 0.0 ||
      result.apparent_distance_deg >= 180.0) {
    result.error =
        "The selected distance limbs produce an invalid centre distance";
    return result;
  }

  const double hm = ToRadians(result.moon_apparent_center_altitude_deg);
  const double hb = ToRadians(result.body_apparent_center_altitude_deg);
  const double denominator = std::cos(hm) * std::cos(hb);
  if (std::fabs(denominator) < 1e-12) {
    result.error = "The measured geometry is singular near the zenith";
    return result;
  }
  const double raw_cos_azimuth =
      (std::cos(ToRadians(result.apparent_distance_deg)) -
       std::sin(hm) * std::sin(hb)) /
      denominator;
  if (raw_cos_azimuth < -1.000001 || raw_cos_azimuth > 1.000001) {
    result.error =
        "The distance and altitudes are geometrically inconsistent (check "
        "limbs and index error)";
    return result;
  }
  const double cos_azimuth = ClampUnit(raw_cos_azimuth);
  result.relative_azimuth_cosine = cos_azimuth;
  result.relative_azimuth_deg = ToDegrees(std::acos(cos_azimuth));

  const double moon_refraction = RefractionDegrees(
      result.moon_apparent_center_altitude_deg, observation.pressure_hpa,
      observation.temperature_c, &result.moon_refraction_x);
  const double body_refraction = RefractionDegrees(
      result.body_apparent_center_altitude_deg, observation.pressure_hpa,
      observation.temperature_c, &result.body_refraction_x);
  if (!Finite(moon_refraction) || !Finite(body_refraction)) {
    result.error = "Atmospheric refraction could not be evaluated";
    return result;
  }
  result.moon_refraction_deg = moon_refraction;
  result.body_refraction_deg = body_refraction;
  const double moon_refracted =
      result.moon_apparent_center_altitude_deg - moon_refraction;
  const double body_refracted =
      result.body_apparent_center_altitude_deg - body_refraction;
  result.moon_topocentric_altitude_deg = moon_refracted;
  result.body_topocentric_altitude_deg = body_refracted;
  result.moon_parallax_in_altitude_deg = ParallaxInAltitude(
      ephemeris.moon_horizontal_parallax_deg, moon_refracted);
  result.body_parallax_in_altitude_deg = ParallaxInAltitude(
      ephemeris.body_horizontal_parallax_deg, body_refracted);
  result.moon_geocentric_altitude_deg =
      moon_refracted + result.moon_parallax_in_altitude_deg;
  result.body_geocentric_altitude_deg =
      body_refracted + result.body_parallax_in_altitude_deg;

  const double hom = ToRadians(result.moon_geocentric_altitude_deg);
  const double hob = ToRadians(result.body_geocentric_altitude_deg);
  const double cos_cleared = std::sin(hom) * std::sin(hob) +
                             std::cos(hom) * std::cos(hob) * cos_azimuth;
  result.cleared_distance_cosine = ClampUnit(cos_cleared);
  result.cleared_distance_deg =
      ToDegrees(std::acos(result.cleared_distance_cosine));
  result.valid = Finite(result.cleared_distance_deg);
  if (!result.valid) result.error = "Cleared lunar distance is not finite";
  return result;
}

SolveResult SolveTime(const Observation& observation,
                      const EphemerisFunction& provider,
                      const SolveOptions& options) {
  if (observation.use_ellipsoid)
    return SolveTimeTagged(observation, provider, options);
  SolveResult result;
  if (!provider) {
    result.error = "No provider is available";
    return result;
  }
  if (!Finite(options.start_offset_seconds) ||
      !Finite(options.end_offset_seconds) ||
      !Finite(options.scan_step_seconds) ||
      !Finite(options.root_tolerance_seconds) ||
      !(options.end_offset_seconds > options.start_offset_seconds) ||
      !(options.scan_step_seconds > 0.0) ||
      !(options.root_tolerance_seconds > 0.0) ||
      !Finite(options.end_offset_seconds - options.start_offset_seconds) ||
      (options.end_offset_seconds - options.start_offset_seconds) /
              options.scan_step_seconds >
          10000.0) {
    result.error = "Invalid lunar-distance search interval";
    return result;
  }

  EphemerisCache ephemeris_cache(provider);
  const EphemerisFunction ephemeris = [&](double t, EphemerisSample* sample,
                                          std::string* error) {
    return ephemeris_cache.Evaluate(t, sample, error);
  };
  struct Point {
    double t;
    double residual;
  };
  std::vector<Point> points;
  double closest_abs = std::numeric_limits<double>::infinity();
  const std::size_t scan_steps = static_cast<std::size_t>(
      std::ceil((options.end_offset_seconds - options.start_offset_seconds) /
                options.scan_step_seconds));
  for (std::size_t index = 0; index < scan_steps; ++index) {
    const double t =
        options.start_offset_seconds + index * options.scan_step_seconds;
    if (t >= options.end_offset_seconds) break;
    if (!points.empty() && t <= points.back().t) continue;
    double residual = 0.0;
    Clearance clearance;
    EphemerisSample sample;
    std::string error;
    if (!EvaluateResidual(observation, ephemeris, t, &residual, &clearance,
                          &sample, &error)) {
      result.error = error;
      return result;
    }
    points.push_back({t, residual});
    result.match_trace.push_back(
        {MatchTracePhase::Scan, 0, static_cast<int>(points.size() - 1), t, t, t,
         sample.predicted_distance_deg, clearance.cleared_distance_deg,
         residual * 60.0});
    if (std::fabs(residual) < closest_abs) {
      closest_abs = std::fabs(residual);
      result.closest_offset_seconds = t;
      result.closest_residual_arcmin = residual * 60.0;
    }
  }
  double end_residual = 0.0;
  Clearance end_clearance;
  EphemerisSample end_sample;
  std::string endpoint_error;
  if (!EvaluateResidual(observation, ephemeris, options.end_offset_seconds,
                        &end_residual, &end_clearance, &end_sample,
                        &endpoint_error)) {
    result.error = endpoint_error;
    return result;
  }
  points.push_back({options.end_offset_seconds, end_residual});
  result.match_trace.push_back(
      {MatchTracePhase::Scan, 0, static_cast<int>(points.size() - 1),
       options.end_offset_seconds, options.end_offset_seconds,
       options.end_offset_seconds, end_sample.predicted_distance_deg,
       end_clearance.cleared_distance_deg, end_residual * 60.0});

  int bracket_number = 0;
  for (std::size_t index = 1; index < points.size(); ++index) {
    Point left = points[index - 1];
    Point right = points[index];
    if (left.residual != 0.0 && right.residual != 0.0 &&
        std::signbit(left.residual) == std::signbit(right.residual))
      continue;
    ++bracket_number;
    if (left.residual == 0.0)
      right = left;
    else if (right.residual == 0.0)
      left = right;
    int refinement_iteration = 0;
    for (int iteration = 0;
         iteration < 80 && right.t - left.t > options.root_tolerance_seconds;
         ++iteration) {
      const double middle_t = 0.5 * (left.t + right.t);
      double middle_residual = 0.0;
      Clearance middle_clearance;
      EphemerisSample middle_sample;
      std::string error;
      if (!EvaluateResidual(observation, ephemeris, middle_t, &middle_residual,
                            &middle_clearance, &middle_sample, &error)) {
        result.error = error;
        return result;
      }
      refinement_iteration = iteration + 1;
      result.match_trace.push_back(
          {MatchTracePhase::Refinement, bracket_number, refinement_iteration,
           middle_t, left.t, right.t, middle_sample.predicted_distance_deg,
           middle_clearance.cleared_distance_deg, middle_residual * 60.0});
      if (middle_residual == 0.0) {
        left = right = {middle_t, middle_residual};
        break;
      }
      if (std::signbit(middle_residual) == std::signbit(left.residual)) {
        left = {middle_t, middle_residual};
      } else {
        right = {middle_t, middle_residual};
      }
    }
    const double root = 0.5 * (left.t + right.t);
    if (!result.candidates.empty() &&
        std::fabs(root - result.candidates.back().offset_seconds) < 1.0)
      continue;
    Clearance clearance;
    EphemerisSample sample;
    double residual = 0.0;
    std::string error;
    if (!EvaluateResidual(observation, ephemeris, root, &residual, &clearance,
                          &sample, &error)) {
      result.error = error;
      return result;
    }
    result.match_trace.push_back(
        {MatchTracePhase::Candidate, bracket_number, refinement_iteration + 1,
         root, left.t, right.t, sample.predicted_distance_deg,
         clearance.cleared_distance_deg, residual * 60.0});
    TimeCandidate candidate;
    candidate.offset_seconds = root;
    candidate.cleared_distance_deg = clearance.cleared_distance_deg;
    candidate.predicted_distance_deg = sample.predicted_distance_deg;
    auto residual_values = [&](double t, std::array<double, 1>* out) {
      return EvaluateResidual(observation, ephemeris, t, &(*out)[0], nullptr,
                              nullptr, &error);
    };
    std::array<double, 1> derivative{};
    candidate.local_slope_available =
        LocalDerivative(residual_values, root, &options, &derivative,
                        &candidate.used_one_sided_slope);
    candidate.slope_arcmin_per_hour =
        candidate.local_slope_available
            ? derivative[0] * 60.0 * 3600.0
            : std::numeric_limits<double>::quiet_NaN();
    double uncertainty_contributions[3] = {};
    candidate.angular_uncertainty_arcmin = AngularUncertainty(
        observation, sample, clearance, uncertainty_contributions);
    candidate.distance_uncertainty_contribution_arcmin =
        uncertainty_contributions[0];
    candidate.moon_altitude_uncertainty_contribution_arcmin =
        uncertainty_contributions[1];
    candidate.body_altitude_uncertainty_contribution_arcmin =
        uncertainty_contributions[2];
    const double slope_arcmin_per_second =
        std::fabs(candidate.slope_arcmin_per_hour) / 3600.0;
    candidate.time_uncertainty_seconds =
        slope_arcmin_per_second > 1e-12
            ? candidate.angular_uncertainty_arcmin / slope_arcmin_per_second
            : std::numeric_limits<double>::infinity();
    const auto position =
        IntersectAltitudeCircles({sample.moon_geographic_latitude_deg,
                                  sample.moon_geographic_longitude_deg},
                                 clearance.moon_geocentric_altitude_deg,
                                 {sample.body_geographic_latitude_deg,
                                  sample.body_geographic_longitude_deg},
                                 clearance.body_geocentric_altitude_deg);
    candidate.positions = position.candidates;
    candidate.position_geometry = position.geometry;
    candidate.circle_crossing_angle_deg = position.circle_crossing_angle_deg;
    // Keep both intersections tied to this UTC candidate. A conservative
    // scalar is the larger of their local horizontal RMS uncertainties.
    candidate.position_uncertainty_nm = 0.0;
    for (const auto& p : position.candidates) {
      const auto uncertainty =
          EstimateTaggedUncertainty(observation, ephemeris, root, p, &options);
      candidate.position_uncertainty_nm =
          std::max(candidate.position_uncertainty_nm, uncertainty.position_nm);
    }
    if (candidate.positions.empty())
      candidate.position_uncertainty_nm =
          std::numeric_limits<double>::infinity();
    candidate.uncertainty_available =
        Finite(candidate.time_uncertainty_seconds) &&
        Finite(candidate.position_uncertainty_nm);
    if (!candidate.local_slope_available)
      result.warnings.push_back(
          "A valid root has no usable local time slope; the slope is "
          "unavailable");
    if (candidate.used_one_sided_slope)
      result.warnings.push_back(
          "A boundary requires a one-sided local time slope");
    if (!candidate.uncertainty_available)
      result.warnings.push_back(
          "A valid root has unavailable local uncertainty; do not treat it as "
          "precise");
    result.candidates.push_back(candidate);
  }

  if (result.candidates.empty()) {
    std::ostringstream message;
    message << "No matching lunar distance occurs in the selected interval; "
            << "closest residual is " << result.closest_residual_arcmin
            << " arcmin";
    result.error = message.str();
    return result;
  }
  if (result.candidates.size() > 1)
    result.warnings.push_back(
        "More than one time matches this observation; use the approximate "
        "date/time or a second lunar to resolve the ambiguity");
  for (const TimeCandidate& candidate : result.candidates) {
    if (std::fabs(candidate.slope_arcmin_per_hour) < 10.0) {
      result.warnings.push_back(
          "The lunar distance is changing slowly, so this geometry gives a "
          "weak time determination");
      break;
    }
  }
  result.valid = true;
  return result;
}

SolveResult SolveTimeTagged(const Observation& observation,
                            const EphemerisFunction& provider,
                            const SolveOptions& options) {
  SolveResult result;
  constexpr std::size_t evaluation_limit = 20000;
  if (!provider || !Finite(options.start_offset_seconds) ||
      !Finite(options.end_offset_seconds) ||
      !Finite(options.scan_step_seconds) ||
      !Finite(options.root_tolerance_seconds) ||
      !(options.end_offset_seconds > options.start_offset_seconds) ||
      !(options.scan_step_seconds > 0.0) ||
      !(options.root_tolerance_seconds > 0.0) ||
      !Finite(options.end_offset_seconds - options.start_offset_seconds) ||
      (options.end_offset_seconds - options.start_offset_seconds) /
              options.scan_step_seconds >
          evaluation_limit / 2) {
    result.error = "Invalid or excessive time-tagged lunar-distance search";
    return result;
  }
  if (!observation.separate_times && !observation.use_ellipsoid) {
    result.error = "Separate angle times are not enabled";
    return result;
  }
  const double inputs[] = {observation.raw_distance_deg,
                           observation.moon_altitude_deg,
                           observation.body_altitude_deg,
                           observation.eye_height_m,
                           observation.index_error_arcmin,
                           observation.pressure_hpa,
                           observation.temperature_c,
                           observation.dip_short_distance_m,
                           observation.distance_uncertainty_arcmin,
                           observation.moon_altitude_uncertainty_arcmin,
                           observation.body_altitude_uncertainty_arcmin,
                           observation.moon_time_offset_seconds,
                           observation.body_time_offset_seconds,
                           observation.course_true_deg,
                           observation.speed_knots};
  if (!std::all_of(std::begin(inputs), std::end(inputs), Finite) ||
      observation.raw_distance_deg <= 0.0 ||
      observation.raw_distance_deg >= 180.0 ||
      observation.moon_altitude_deg <= -90.0 ||
      observation.moon_altitude_deg >= 180.0 ||
      observation.body_altitude_deg <= -90.0 ||
      observation.body_altitude_deg >= 180.0 ||
      observation.eye_height_m < 0.0 || observation.pressure_hpa <= 0.0 ||
      observation.temperature_c <= -100.0 || observation.speed_knots < 0.0 ||
      observation.distance_uncertainty_arcmin < 0.0 ||
      observation.moon_altitude_uncertainty_arcmin < 0.0 ||
      observation.body_altitude_uncertainty_arcmin < 0.0 ||
      (observation.dip_short && observation.dip_short_distance_m <= 0.0)) {
    result.error =
        "A time-tagged lunar observation input is outside its usable range";
    return result;
  }
  EphemerisCache ephemeris_cache(provider);
  const EphemerisFunction ephemeris = [&](double t, EphemerisSample* sample,
                                          std::string* error) {
    return ephemeris_cache.Evaluate(t, sample, error);
  };
  auto warn = [&](const std::string& message) {
    if (std::find(result.warnings.begin(), result.warnings.end(), message) ==
        result.warnings.end())
      result.warnings.push_back(message);
  };
  std::map<double, TaggedEvaluation> cache;
  bool exhausted = false;
  TaggedEvaluation unavailable;
  double closest = std::numeric_limits<double>::infinity();
  auto evaluate = [&](double t) -> const TaggedEvaluation& {
    const auto existing = cache.find(t);
    if (existing != cache.end()) return existing->second;
    if (cache.size() >= evaluation_limit) {
      exhausted = true;
      return unavailable;
    }
    auto entry = cache.emplace(t, EvaluateTagged(observation, ephemeris, t));
    const auto& e = entry.first->second;
    if (e.valid)
      for (std::size_t i = 0; i < e.residuals.size(); ++i) {
        const double residual = e.residuals[i];
        result.match_trace.push_back(
            {MatchTracePhase::Scan, e.branch_ids[i] + 1,
             static_cast<int>(cache.size()), t, t, t,
             observation.raw_distance_deg + residual,
             observation.raw_distance_deg, residual * 60.0});
        if (std::fabs(residual) < closest) {
          closest = std::fabs(residual);
          result.closest_offset_seconds = t;
          result.closest_residual_arcmin = residual * 60.0;
        }
      }
    return e;
  };
  // This is a floating-point zero test, not an observational acceptance band.
  const double angular_zero = 8.0 * std::numeric_limits<double>::epsilon() *
                              std::max(1.0, observation.raw_distance_deg);
  int bracket_number = 0;
  auto accept = [&](double root, int id, double left, double right,
                    int iteration) {
    const auto& e = evaluate(root);
    const int branch = BranchIndex(e, id);
    if (branch < 0 || std::fabs(e.residuals[branch]) > kTaggedMatchToleranceDeg)
      return;
    for (const auto& candidate : result.candidates)
      if (std::fabs(candidate.offset_seconds - root) <
          2.0 * options.root_tolerance_seconds)
        for (const auto& p : candidate.positions)
          if (GreatCircleDistanceNm(p, e.positions[branch]) < 1e-5) return;
    result.match_trace.push_back(
        {MatchTracePhase::Candidate, ++bracket_number, iteration, root, left,
         right, observation.raw_distance_deg + e.residuals[branch],
         observation.raw_distance_deg, e.residuals[branch] * 60.0});
    TimeCandidate candidate;
    candidate.offset_seconds = root;
    candidate.predicted_distance_deg = e.distance_sample.predicted_distance_deg;
    candidate.cleared_distance_deg = e.distance_sample.predicted_distance_deg;
    candidate.positions.push_back(e.positions[branch]);
    candidate.position_geometry.push_back(e.geometry[branch]);
    candidate.circle_crossing_angle_deg =
        e.geometry[branch].effective_crossing_angle_deg;
    auto residual = [&](double t, std::array<double, 1>* value) {
      const auto& p = evaluate(t);
      const int i = BranchIndex(p, id);
      if (i < 0) return false;
      (*value)[0] = p.residuals[i];
      return true;
    };
    std::array<double, 1> derivative{};
    candidate.local_slope_available = LocalDerivative(
        residual, root, &options, &derivative, &candidate.used_one_sided_slope);
    candidate.slope_arcmin_per_hour =
        candidate.local_slope_available
            ? derivative[0] * 60.0 * 3600.0
            : std::numeric_limits<double>::quiet_NaN();
    candidate.angular_uncertainty_arcmin =
        std::hypot(observation.distance_uncertainty_arcmin,
                   std::hypot(observation.moon_altitude_uncertainty_arcmin,
                              observation.body_altitude_uncertainty_arcmin));
    const auto uncertainty = EstimateTaggedUncertainty(
        observation, ephemeris, root, candidate.positions.front(), &options);
    candidate.uncertainty_available = uncertainty.valid;
    candidate.time_uncertainty_seconds = uncertainty.time_seconds;
    candidate.position_uncertainty_nm = uncertainty.position_nm;
    if (!candidate.local_slope_available)
      warn(
          "A valid root has no usable local time slope; the slope is "
          "unavailable");
    if (candidate.used_one_sided_slope)
      warn("A boundary requires a one-sided local time slope");
    if (!candidate.uncertainty_available)
      warn(
          "A valid root has unavailable local uncertainty; do not treat it as "
          "precise");
    if (candidate.circle_crossing_angle_deg < 1.0)
      warn(
          "The altitude constraints meet at less than one degree; position "
          "geometry is weak");
    if (candidate.local_slope_available &&
        std::fabs(candidate.slope_arcmin_per_hour) < 10.0)
      warn(
          "The lunar distance is changing slowly, so this geometry gives a "
          "weak time determination");
    for (auto& existing : result.candidates) {
      if (std::fabs(existing.offset_seconds - root) <
          2.0 * options.root_tolerance_seconds) {
        // Merged branches must describe the retained UTC, rather than mixing
        // the position at one refined root with the time of another root.
        const auto& at_existing_time = evaluate(existing.offset_seconds);
        const int existing_branch = BranchIndex(at_existing_time, id);
        if (existing_branch < 0 ||
            std::fabs(at_existing_time.residuals[existing_branch]) >
                kTaggedMatchToleranceDeg)
          continue;
        const auto& position = at_existing_time.positions[existing_branch];
        bool duplicate = false;
        for (const auto& p : existing.positions)
          if (GreatCircleDistanceNm(p, position) < 1e-5) duplicate = true;
        if (duplicate) return;
        existing.positions.push_back(position);
        existing.position_geometry.push_back(
            at_existing_time.geometry[existing_branch]);
        const auto merged_uncertainty = EstimateTaggedUncertainty(
            observation, ephemeris, existing.offset_seconds, position,
            &options);
        existing.time_uncertainty_seconds = std::max(
            existing.time_uncertainty_seconds, merged_uncertainty.time_seconds);
        existing.position_uncertainty_nm = std::max(
            existing.position_uncertainty_nm, merged_uncertainty.position_nm);
        existing.uncertainty_available =
            existing.uncertainty_available && merged_uncertainty.valid;
        if (!merged_uncertainty.valid)
          warn(
              "A valid root has unavailable local uncertainty; do not treat it "
              "as precise");
        return;
      }
    }
    result.candidates.push_back(std::move(candidate));
  };
  std::function<void(double, double, int, int)> search;
  search = [&](double start, double end, int blind_depth, int depth) {
    if (exhausted || depth >= 48) return;
    const auto& a = evaluate(start);
    const auto& b = evaluate(end);
    for (int id : a.branch_ids) {
      const int i = BranchIndex(a, id);
      if (i >= 0 && std::fabs(a.residuals[i]) <= angular_zero)
        accept(start, id, start, start, depth);
    }
    for (int id : b.branch_ids) {
      const int i = BranchIndex(b, id);
      if (i >= 0 && std::fabs(b.residuals[i]) <= angular_zero)
        accept(end, id, end, end, depth);
    }
    const double middle = start + (end - start) * 0.5;
    if (middle == start || middle == end) return;
    // Follow a feasibility boundary to a fraction of the root tolerance. A
    // small, bounded probe of fully invalid intervals can discover an island;
    // finite sampling cannot promise discovery of arbitrarily narrow islands.
    if (!a.valid || !b.valid || a.branch_ids != b.branch_ids) {
      if (end - start <= options.root_tolerance_seconds * 0.25) return;
      if (!a.valid && !b.valid && blind_depth >= 2) return;
      evaluate(middle);
      const int next_blind = (!a.valid && !b.valid) ? blind_depth + 1 : 0;
      search(start, middle, next_blind, depth + 1);
      search(middle, end, next_blind, depth + 1);
      return;
    }
    for (int id : a.branch_ids) {
      const int first = BranchIndex(a, id), last = BranchIndex(b, id);
      if (first < 0 || last < 0) continue;
      double left = start, right = end;
      double fleft = a.residuals[first], fright = b.residuals[last];
      if (std::fabs(fleft) <= angular_zero ||
          std::fabs(fright) <= angular_zero ||
          std::signbit(fleft) == std::signbit(fright))
        continue;
      bool bracket_valid = true;
      int iteration = 0;
      double selected_root = std::numeric_limits<double>::quiet_NaN();
      for (; iteration < 80; ++iteration) {
        const double t = left + (right - left) * 0.5;
        if (t == left || t == right) break;
        const auto& e = evaluate(t);
        const int i = BranchIndex(e, id);
        if (i < 0) {
          // Never bisect across a missing branch/provider gap. Explore each
          // feasible side separately instead of joining unrelated residuals.
          search(left, t, 0, depth + 1);
          search(t, right, 0, depth + 1);
          bracket_valid = false;
          break;
        }
        const double f = e.residuals[i];
        result.match_trace.push_back({MatchTracePhase::Refinement, id + 1,
                                      iteration, t, left, right,
                                      observation.raw_distance_deg + f,
                                      observation.raw_distance_deg, f * 60.0});
        if (std::fabs(f) <= angular_zero ||
            (right - left <= options.root_tolerance_seconds &&
             std::fabs(f) <= kTaggedMatchToleranceDeg)) {
          selected_root = t;
          break;
        }
        if (std::signbit(f) == std::signbit(fleft)) {
          left = t;
          fleft = f;
        } else {
          right = t;
          fright = f;
        }
      }
      if (bracket_valid && (Finite(selected_root) ||
                            right - left <= options.root_tolerance_seconds))
        accept(
            Finite(selected_root) ? selected_root : left + (right - left) * 0.5,
            id, left, right, iteration);
    }
  };
  std::vector<double> grid;
  const double span = options.end_offset_seconds - options.start_offset_seconds;
  const std::size_t steps =
      static_cast<std::size_t>(std::ceil(span / options.scan_step_seconds));
  for (std::size_t i = 0; i < steps; ++i) {
    const double t =
        options.start_offset_seconds + i * options.scan_step_seconds;
    if (t < options.end_offset_seconds && (grid.empty() || t > grid.back())) {
      grid.push_back(t);
      evaluate(t);
    }
  }
  grid.push_back(options.end_offset_seconds);
  evaluate(grid.back());
  for (std::size_t i = 1; i < grid.size() && !exhausted; ++i)
    search(grid[i - 1], grid[i], 0, 0);
  if (exhausted) {
    result.candidates.clear();
    result.error =
        "Lunar search evaluation limit exceeded; narrow the search interval";
    return result;
  }
  std::sort(result.candidates.begin(), result.candidates.end(),
            [](const TimeCandidate& a, const TimeCandidate& b) {
              return a.offset_seconds < b.offset_seconds;
            });
  if (result.candidates.empty()) {
    std::ostringstream message;
    message << "No joint UTC/position solution occurs in the selected interval";
    if (Finite(closest))
      message << "; closest lunar-distance residual is "
              << result.closest_residual_arcmin << " arcmin";
    result.error = message.str();
    return result;
  }
  if (result.candidates.size() > 1)
    warn(
        "More than one joint time/position solution exists; use DR, "
        "hemisphere, another altitude or a second lunar to resolve it");
  result.valid = true;
  return result;
}

PredictedObservation PredictTimeTaggedObservation(
    const Observation& settings, const EphemerisFunction& ephemeris,
    double clock_correction_seconds,
    const GeographicPoint& reference_position) {
  PredictedObservation result;
  EphemerisSample distance_sample;
  EphemerisSample moon_sample;
  EphemerisSample body_sample;
  if (!ephemeris) {
    result.error = "No ephemeris is available";
    return result;
  }
  if (!ephemeris(clock_correction_seconds, &distance_sample, &result.error) ||
      !ephemeris(clock_correction_seconds + settings.moon_time_offset_seconds,
                 &moon_sample, &result.error) ||
      !ephemeris(clock_correction_seconds + settings.body_time_offset_seconds,
                 &body_sample, &result.error))
    return result;
  const GeographicPoint moon_observer = ObserverAt(
      reference_position, settings, settings.moon_time_offset_seconds);
  const GeographicPoint body_observer = ObserverAt(
      reference_position, settings, settings.body_time_offset_seconds);
  result.raw_distance_deg =
      PredictedRawDistance(settings, distance_sample, reference_position);
  result.moon_altitude_deg =
      PredictedRawAltitude(settings, moon_sample, moon_observer, true);
  result.body_altitude_deg =
      PredictedRawAltitude(settings, body_sample, body_observer, false);
  result.valid = Finite(result.raw_distance_deg) &&
                 Finite(result.moon_altitude_deg) &&
                 Finite(result.body_altitude_deg);
  if (!result.valid) result.error = "A predicted sextant angle is not finite";
  return result;
}

PositionResult PositionAtTime(const Observation& observation,
                              const EphemerisFunction& ephemeris,
                              double correction_seconds) {
  PositionResult result;
  if (!ephemeris) {
    result.error = "No ephemeris is available";
    return result;
  }
  if (observation.separate_times || observation.use_ellipsoid) {
    const auto evaluation =
        EvaluateTagged(observation, ephemeris, correction_seconds);
    result.valid = evaluation.valid;
    result.error = evaluation.error;
    result.candidates = evaluation.positions;
    result.geometry = evaluation.geometry;
    return result;
  }
  EphemerisSample sample;
  if (!ephemeris(correction_seconds, &sample, &result.error)) return result;
  const auto clearance = ClearDistance(observation, sample);
  if (!clearance.valid) {
    result.error = clearance.error;
    return result;
  }
  return IntersectAltitudeCircles({sample.moon_geographic_latitude_deg,
                                   sample.moon_geographic_longitude_deg},
                                  clearance.moon_geocentric_altitude_deg,
                                  {sample.body_geographic_latitude_deg,
                                   sample.body_geographic_longitude_deg},
                                  clearance.body_geocentric_altitude_deg);
}

PositionResult IntersectAltitudeCircles(
    const GeographicPoint& moon_geographic_position,
    double moon_observed_altitude_deg,
    const GeographicPoint& body_geographic_position,
    double body_observed_altitude_deg) {
  PositionResult result;
  if (!Finite(moon_geographic_position.latitude_deg) ||
      !Finite(moon_geographic_position.longitude_deg) ||
      !Finite(body_geographic_position.latitude_deg) ||
      !Finite(body_geographic_position.longitude_deg) ||
      !Finite(moon_observed_altitude_deg) ||
      !Finite(body_observed_altitude_deg) ||
      std::fabs(moon_geographic_position.latitude_deg) > 90.0 ||
      std::fabs(body_geographic_position.latitude_deg) > 90.0 ||
      moon_observed_altitude_deg <= -90.0 ||
      moon_observed_altitude_deg >= 90.0 ||
      body_observed_altitude_deg <= -90.0 ||
      body_observed_altitude_deg >= 90.0) {
    result.error = "Invalid altitude-circle input";
    return result;
  }
  const Vector3 moon = Unit(moon_geographic_position);
  const Vector3 body = Unit(body_geographic_position);
  const double dot = ClampUnit(Dot(moon, body));
  const double denominator = 1.0 - dot * dot;
  if (denominator < 1e-12) {
    result.error =
        "The two celestial geographic positions are coincident or antipodal";
    return result;
  }
  const double moon_plane = std::sin(ToRadians(moon_observed_altitude_deg));
  const double body_plane = std::sin(ToRadians(body_observed_altitude_deg));
  const double a = (moon_plane - dot * body_plane) / denominator;
  const double b = (body_plane - dot * moon_plane) / denominator;
  const Vector3 base = a * moon + b * body;
  const double remaining = 1.0 - Dot(base, base);
  if (remaining < -1e-10) {
    result.error =
        "The corrected Moon and body altitude circles do not intersect; check "
        "observations and limb corrections";
    return result;
  }
  Vector3 normal = Cross(moon, body);
  const double normal_norm = Norm(normal);
  normal = (1.0 / normal_norm) * normal;
  const double scale = std::sqrt(std::max(0.0, remaining));
  const Vector3 first = base + scale * normal;
  const Vector3 second = base + (-scale) * normal;
  result.candidates.push_back(Geographic(first));
  if (scale > 1e-10) result.candidates.push_back(Geographic(second));

  for (const GeographicPoint& candidate : result.candidates)
    result.geometry.push_back(CalculatePositionGeometry(
        candidate, moon_geographic_position, body_geographic_position));

  if (!result.geometry.empty())
    result.circle_crossing_angle_deg =
        result.geometry.front().effective_crossing_angle_deg;
  result.valid = true;
  return result;
}

double GreatCircleDistanceNm(const GeographicPoint& first,
                             const GeographicPoint& second) {
  const double latitude_difference =
      ToRadians(second.latitude_deg - first.latitude_deg);
  const double longitude_difference =
      ToRadians(second.longitude_deg - first.longitude_deg);
  const double first_latitude = ToRadians(first.latitude_deg);
  const double second_latitude = ToRadians(second.latitude_deg);
  const double haversine = std::sin(latitude_difference / 2.0) *
                               std::sin(latitude_difference / 2.0) +
                           std::cos(first_latitude) *
                               std::cos(second_latitude) *
                               std::sin(longitude_difference / 2.0) *
                               std::sin(longitude_difference / 2.0);
  return ToDegrees(2.0 *
                   std::atan2(std::sqrt(std::max(0.0, haversine)),
                              std::sqrt(std::max(0.0, 1.0 - haversine)))) *
         60.0;
}

}  // namespace lunar_distance
