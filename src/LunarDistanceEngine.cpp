#include "LunarDistanceEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace lunar_distance {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;

double ToRadians(double degrees) { return degrees * kPi / 180.0; }
double ToDegrees(double radians) { return radians * 180.0 / kPi; }
double ClampUnit(double value) {
  return std::max(-1.0, std::min(1.0, value));
}

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
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
          a.x * b.y - a.y * b.x};
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
  point.latitude_deg = ToDegrees(std::atan2(value.z, std::hypot(value.x, value.y)));
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

double RefractionDegrees(double apparent_altitude_deg, double pressure_hpa,
                         double temperature_c) {
  // Same Bennett/Saemundsson form used by the rest of the plugin. It is not
  // reliable at or below the astronomical horizon.
  const double altitude = ToRadians(apparent_altitude_deg);
  const double denominator =
      std::tan(altitude) + 0.028;
  if (std::fabs(denominator) < 1e-12) return std::numeric_limits<double>::quiet_NaN();
  const double x = std::tan(altitude + ToRadians(0.04848) / denominator);
  if (!Finite(x) || std::fabs(x) < 1e-12 || temperature_c <= -273.15)
    return std::numeric_limits<double>::quiet_NaN();
  return 0.267 * pressure_hpa / (x * (temperature_c + 273.15)) / 60.0;
}

double ParallaxInAltitude(double horizontal_parallax_deg,
                          double altitude_deg) {
  return ToDegrees(std::asin(ClampUnit(
      std::sin(ToRadians(horizontal_parallax_deg)) *
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
                          const Clearance& nominal) {
  const double inputs[3] = {observation.distance_uncertainty_arcmin,
                            observation.moon_altitude_uncertainty_arcmin,
                            observation.body_altitude_uncertainty_arcmin};
  double variance = 0.0;
  for (int index = 0; index < 3; ++index) {
    if (!(inputs[index] > 0.0)) continue;
    Observation perturbed = observation;
    const double step_deg = std::max(1e-5, inputs[index] / 60.0);
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
    variance += sensitivity * sensitivity * inputs[index] * inputs[index];
  }
  return std::sqrt(variance);
}

}  // namespace

Clearance ClearDistance(const Observation& observation,
                        const EphemerisSample& ephemeris) {
  Clearance result;
  const double values[] = {
      observation.raw_distance_deg, observation.moon_altitude_deg,
      observation.body_altitude_deg, observation.index_error_arcmin,
      observation.eye_height_m, observation.pressure_hpa,
      observation.temperature_c, ephemeris.predicted_distance_deg,
      ephemeris.moon_semidiameter_deg,
      ephemeris.moon_horizontal_parallax_deg,
      ephemeris.body_semidiameter_deg,
      ephemeris.body_horizontal_parallax_deg};
  for (double value : values) {
    if (!Finite(value)) {
      result.error = "A lunar-distance input is not finite";
      return result;
    }
  }
  if (observation.raw_distance_deg <= 0.0 ||
      observation.raw_distance_deg >= 180.0) {
    result.error = "The measured lunar distance must be between 0 and 180 degrees";
    return result;
  }
  if (observation.eye_height_m < 0.0 || observation.pressure_hpa <= 0.0 ||
      observation.temperature_c <= -100.0) {
    result.error = "Eye height, pressure or temperature is outside a usable range";
    return result;
  }
  if (observation.dip_short && observation.dip_short_distance_m <= 0.0) {
    result.error = "Dip-short distance must be greater than zero";
    return result;
  }

  const double index_correction_deg = observation.index_error_arcmin / 60.0;
  double dip_deg = 0.0;
  if (!observation.artificial_horizon) {
    dip_deg = observation.dip_short
                  ? (0.4156 * observation.dip_short_distance_m +
                     1.856 * observation.eye_height_m /
                         observation.dip_short_distance_m) /
                        60.0
                  : 1.758 * std::sqrt(observation.eye_height_m) / 60.0;
  }

  auto apparent_limb_altitude = [&](double hs) {
    double value = hs - index_correction_deg - dip_deg;
    if (observation.artificial_horizon) value *= 0.5;
    return value;
  };

  const double moon_limb_alt =
      apparent_limb_altitude(observation.moon_altitude_deg);
  const double body_limb_alt =
      apparent_limb_altitude(observation.body_altitude_deg);
  const double moon_topocentric_sd =
      ephemeris.moon_semidiameter_deg *
      (1.0 + std::sin(ToRadians(moon_limb_alt)) *
                 std::sin(ToRadians(ephemeris.moon_horizontal_parallax_deg)));
  result.moon_apparent_center_altitude_deg =
      moon_limb_alt + AltitudeLimbSign(observation.moon_altitude_limb) *
                          moon_topocentric_sd;
  result.body_apparent_center_altitude_deg =
      body_limb_alt + AltitudeLimbSign(observation.body_altitude_limb) *
                          ephemeris.body_semidiameter_deg;

  if (result.moon_apparent_center_altitude_deg <= -1.0 ||
      result.body_apparent_center_altitude_deg <= -1.0 ||
      result.moon_apparent_center_altitude_deg >= 90.0 ||
      result.body_apparent_center_altitude_deg >= 90.0) {
    result.error =
        "Both body centres must be above the usable astronomical horizon";
    return result;
  }

  result.apparent_distance_deg =
      observation.raw_distance_deg - index_correction_deg +
      ContactSign(observation.moon_contact) * moon_topocentric_sd +
      ContactSign(observation.body_contact) *
          ephemeris.body_semidiameter_deg;
  if (observation.artificial_horizon) {
    // The artificial horizon doubles altitudes, not the inter-body distance.
  }
  if (result.apparent_distance_deg <= 0.0 ||
      result.apparent_distance_deg >= 180.0) {
    result.error = "The selected distance limbs produce an invalid centre distance";
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
        "The distance and altitudes are geometrically inconsistent (check limbs and index error)";
    return result;
  }
  const double cos_azimuth = ClampUnit(raw_cos_azimuth);
  result.relative_azimuth_deg = ToDegrees(std::acos(cos_azimuth));

  const double moon_refraction = RefractionDegrees(
      result.moon_apparent_center_altitude_deg, observation.pressure_hpa,
      observation.temperature_c);
  const double body_refraction = RefractionDegrees(
      result.body_apparent_center_altitude_deg, observation.pressure_hpa,
      observation.temperature_c);
  if (!Finite(moon_refraction) || !Finite(body_refraction)) {
    result.error = "Atmospheric refraction could not be evaluated";
    return result;
  }
  const double moon_refracted =
      result.moon_apparent_center_altitude_deg - moon_refraction;
  const double body_refracted =
      result.body_apparent_center_altitude_deg - body_refraction;
  result.moon_geocentric_altitude_deg =
      moon_refracted + ParallaxInAltitude(
                           ephemeris.moon_horizontal_parallax_deg,
                           moon_refracted);
  result.body_geocentric_altitude_deg =
      body_refracted + ParallaxInAltitude(
                           ephemeris.body_horizontal_parallax_deg,
                           body_refracted);

  const double hom = ToRadians(result.moon_geocentric_altitude_deg);
  const double hob = ToRadians(result.body_geocentric_altitude_deg);
  const double cos_cleared =
      std::sin(hom) * std::sin(hob) +
      std::cos(hom) * std::cos(hob) * cos_azimuth;
  result.cleared_distance_deg = ToDegrees(std::acos(ClampUnit(cos_cleared)));
  result.valid = Finite(result.cleared_distance_deg);
  if (!result.valid) result.error = "Cleared lunar distance is not finite";
  return result;
}

SolveResult SolveTime(const Observation& observation,
                      const EphemerisFunction& ephemeris,
                      const SolveOptions& options) {
  SolveResult result;
  if (!ephemeris) {
    result.error = "No ephemeris is available";
    return result;
  }
  if (!(options.end_offset_seconds > options.start_offset_seconds) ||
      !(options.scan_step_seconds > 0.0) ||
      !(options.root_tolerance_seconds > 0.0)) {
    result.error = "Invalid lunar-distance search interval";
    return result;
  }

  struct Point {
    double t;
    double residual;
  };
  std::vector<Point> points;
  double closest_abs = std::numeric_limits<double>::infinity();
  for (double t = options.start_offset_seconds;
       t < options.end_offset_seconds; t += options.scan_step_seconds) {
    double residual = 0.0;
    std::string error;
    if (!EvaluateResidual(observation, ephemeris, t, &residual, nullptr,
                          nullptr, &error)) {
      result.error = error;
      return result;
    }
    points.push_back({t, residual});
    if (std::fabs(residual) < closest_abs) {
      closest_abs = std::fabs(residual);
      result.closest_offset_seconds = t;
      result.closest_residual_arcmin = residual * 60.0;
    }
  }
  double end_residual = 0.0;
  std::string endpoint_error;
  if (!EvaluateResidual(observation, ephemeris, options.end_offset_seconds,
                        &end_residual, nullptr, nullptr, &endpoint_error)) {
    result.error = endpoint_error;
    return result;
  }
  points.push_back({options.end_offset_seconds, end_residual});

  for (std::size_t index = 1; index < points.size(); ++index) {
    Point left = points[index - 1];
    Point right = points[index];
    if (left.residual != 0.0 && right.residual != 0.0 &&
        std::signbit(left.residual) == std::signbit(right.residual))
      continue;
    for (int iteration = 0;
         iteration < 80 && right.t - left.t > options.root_tolerance_seconds;
         ++iteration) {
      const double middle_t = 0.5 * (left.t + right.t);
      double middle_residual = 0.0;
      std::string error;
      if (!EvaluateResidual(observation, ephemeris, middle_t,
                            &middle_residual, nullptr, nullptr, &error)) {
        result.error = error;
        return result;
      }
      if (middle_residual == 0.0 ||
          std::signbit(middle_residual) == std::signbit(left.residual)) {
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
    const double derivative_interval = 30.0;
    double before = 0.0, after = 0.0;
    if (!EvaluateResidual(observation, ephemeris, root - derivative_interval,
                          &before, nullptr, nullptr, &error) ||
        !EvaluateResidual(observation, ephemeris, root + derivative_interval,
                          &after, nullptr, nullptr, &error)) {
      result.error = error;
      return result;
    }
    TimeCandidate candidate;
    candidate.offset_seconds = root;
    candidate.cleared_distance_deg = clearance.cleared_distance_deg;
    candidate.predicted_distance_deg = sample.predicted_distance_deg;
    candidate.slope_arcmin_per_hour =
        (after - before) * 60.0 * 3600.0 / (2.0 * derivative_interval);
    candidate.angular_uncertainty_arcmin =
        AngularUncertainty(observation, sample, clearance);
    const double slope_arcmin_per_second =
        std::fabs(candidate.slope_arcmin_per_hour) / 3600.0;
    candidate.time_uncertainty_seconds =
        slope_arcmin_per_second > 1e-12
            ? candidate.angular_uncertainty_arcmin / slope_arcmin_per_second
            : std::numeric_limits<double>::infinity();
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
        "More than one time matches this observation; use the approximate date/time or a second lunar to resolve the ambiguity");
  for (const TimeCandidate& candidate : result.candidates) {
    if (std::fabs(candidate.slope_arcmin_per_hour) < 10.0) {
      result.warnings.push_back(
          "The lunar distance is changing slowly, so this geometry gives a weak time determination");
      break;
    }
  }
  result.valid = true;
  return result;
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
      moon_observed_altitude_deg <= -90.0 || moon_observed_altitude_deg >= 90.0 ||
      body_observed_altitude_deg <= -90.0 || body_observed_altitude_deg >= 90.0) {
    result.error = "Invalid altitude-circle input";
    return result;
  }
  const Vector3 moon = Unit(moon_geographic_position);
  const Vector3 body = Unit(body_geographic_position);
  const double dot = ClampUnit(Dot(moon, body));
  const double denominator = 1.0 - dot * dot;
  if (denominator < 1e-12) {
    result.error = "The two celestial geographic positions are coincident or antipodal";
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
        "The corrected Moon and body altitude circles do not intersect; check observations and limb corrections";
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

  const Vector3 observer = first;
  const Vector3 moon_tangent = moon + (-moon_plane) * observer;
  const Vector3 body_tangent = body + (-body_plane) * observer;
  const double tangent_denominator = Norm(moon_tangent) * Norm(body_tangent);
  if (tangent_denominator > 0.0) {
    double angle = ToDegrees(std::acos(ClampUnit(
        Dot(moon_tangent, body_tangent) / tangent_denominator)));
    if (angle > 90.0) angle = 180.0 - angle;
    result.circle_crossing_angle_deg = angle;
  }
  result.valid = true;
  return result;
}

double GreatCircleDistanceNm(const GeographicPoint& first,
                             const GeographicPoint& second) {
  const Vector3 a = Unit(first);
  const Vector3 b = Unit(second);
  return ToDegrees(std::acos(ClampUnit(Dot(a, b)))) * 60.0;
}

}  // namespace lunar_distance
