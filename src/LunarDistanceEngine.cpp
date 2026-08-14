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
  const AltitudeLimb limb = moon ? observation.moon_altitude_limb
                                 : observation.body_altitude_limb;
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
  const double sd = moon
                        ? nominal_sd *
                              (1.0 + std::sin(ToRadians(limb_altitude)) *
                                         std::sin(ToRadians(hp)))
                        : nominal_sd;
  const double apparent_center =
      limb_altitude + AltitudeLimbSign(limb) * sd;
  const double refraction = RefractionDegrees(
      apparent_center, observation.pressure_hpa, observation.temperature_c);
  if (!Finite(refraction) || apparent_center <= -1.0 ||
      apparent_center >= 90.0) {
    if (error) *error = "An altitude is outside the usable refraction range";
    return false;
  }
  const double topocentric = apparent_center - refraction;
  *geocentric_altitude_deg =
      topocentric + ParallaxInAltitude(hp, topocentric);
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
  const double destination_latitude = std::asin(ClampUnit(
      std::sin(latitude) * std::cos(angular) +
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
  return Destination(reference, bearing, distance);
}

void AltitudeAzimuth(const GeographicPoint& observer,
                     const GeographicPoint& geographic_position,
                     double* altitude_deg, double* azimuth_deg) {
  const double latitude = ToRadians(observer.latitude_deg);
  const double declination = ToRadians(geographic_position.latitude_deg);
  const double longitude_difference =
      ToRadians(geographic_position.longitude_deg - observer.longitude_deg);
  const double sine_altitude =
      std::sin(latitude) * std::sin(declination) +
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
  const AltitudeLimb limb = moon ? observation.moon_altitude_limb
                                 : observation.body_altitude_limb;
  const double topocentric = TopocentricFromGeocentric(geocentric, hp);
  const double apparent_center =
      ApparentFromTopocentric(topocentric, observation);
  if (!Finite(apparent_center))
    return std::numeric_limits<double>::quiet_NaN();
  double limb_altitude =
      apparent_center - AltitudeLimbSign(limb) * nominal_sd;
  if (moon) {
    for (int iteration = 0; iteration < 5; ++iteration) {
      const double topocentric_sd =
          nominal_sd *
          (1.0 + std::sin(ToRadians(limb_altitude)) *
                     std::sin(ToRadians(hp)));
      limb_altitude =
          apparent_center - AltitudeLimbSign(limb) * topocentric_sd;
    }
  }
  const double corrected = observation.artificial_horizon
                               ? 2.0 * limb_altitude
                               : limb_altitude;
  return corrected + observation.index_error_arcmin / 60.0 +
         DipDegrees(observation);
}

double PredictedRawDistance(const Observation& observation,
                            const EphemerisSample& sample,
                            const GeographicPoint& observer) {
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
      std::sin(ToRadians(moon_apparent)) *
          std::sin(ToRadians(body_apparent)) +
      std::cos(ToRadians(moon_apparent)) *
          std::cos(ToRadians(body_apparent)) * std::cos(azimuth_difference))));
  const double moon_sd =
      sample.moon_semidiameter_deg *
      (1.0 + std::sin(ToRadians(moon_apparent)) *
                 std::sin(ToRadians(sample.moon_horizontal_parallax_deg)));
  return apparent_distance + observation.index_error_arcmin / 60.0 -
         ContactSign(observation.moon_contact) * moon_sd -
         ContactSign(observation.body_contact) *
             sample.body_semidiameter_deg;
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
    double body_altitude, std::string* error) {
  const GeographicPoint moon_gp{moon_sample.moon_geographic_latitude_deg,
                                 moon_sample.moon_geographic_longitude_deg};
  const GeographicPoint body_gp{body_sample.body_geographic_latitude_deg,
                                 body_sample.body_geographic_longitude_deg};
  PositionResult seeds = IntersectAltitudeCircles(
      moon_gp, moon_altitude, body_gp, body_altitude);
  if (!seeds.valid) {
    if (error) *error = seeds.error;
    return {};
  }
  if (!observation.moving_observer || observation.speed_knots == 0.0)
    return seeds.candidates;

  std::vector<GeographicPoint> result;
  for (GeographicPoint position : seeds.candidates) {
    bool converged = false;
    for (int iteration = 0; iteration < 30; ++iteration) {
      const GeographicPoint moon_observer = ObserverAt(
          position, observation, observation.moon_time_offset_seconds);
      const GeographicPoint body_observer = ObserverAt(
          position, observation, observation.body_time_offset_seconds);
      const double f0 =
          CalculatedGeocentricAltitude(moon_observer, moon_gp) - moon_altitude;
      const double f1 =
          CalculatedGeocentricAltitude(body_observer, body_gp) - body_altitude;
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
      const double j00 =
          (CalculatedGeocentricAltitude(moon_lat_observer, moon_gp) -
           moon_altitude - f0) /
          step;
      const double j10 =
          (CalculatedGeocentricAltitude(body_lat_observer, body_gp) -
           body_altitude - f1) /
          step;
      const double j01 =
          (CalculatedGeocentricAltitude(moon_lon_observer, moon_gp) -
           moon_altitude - f0) /
          step;
      const double j11 =
          (CalculatedGeocentricAltitude(body_lon_observer, body_gp) -
           body_altitude - f1) /
          step;
      const double determinant = j00 * j11 - j01 * j10;
      if (std::fabs(determinant) < 1e-12) break;
      double dlat = (-f0 * j11 + j01 * f1) / determinant;
      double dlon = (-j00 * f1 + f0 * j10) / determinant;
      const double damping =
          std::max(1.0, std::max(std::fabs(dlat), std::fabs(dlon)) / 2.0);
      position.latitude_deg += dlat / damping;
      position.longitude_deg =
          NormalizeLongitude(position.longitude_deg + dlon / damping);
      if (position.latitude_deg <= -89.999 ||
          position.latitude_deg >= 89.999)
        break;
    }
    if (!converged) continue;
    bool duplicate = false;
    for (const GeographicPoint& existing : result)
      if (ToDegrees(std::acos(ClampUnit(Dot(Unit(existing), Unit(position))))) <
          1e-6)
        duplicate = true;
    if (!duplicate) result.push_back(position);
  }
  if (result.empty() && error)
    *error = "The moving-observer altitude circles did not converge";
  return result;
}

struct TaggedEvaluation {
  bool valid = false;
  std::string error;
  EphemerisSample distance_sample;
  std::vector<GeographicPoint> positions;
  std::vector<double> residuals;
};

TaggedEvaluation EvaluateTagged(const Observation& observation,
                                const EphemerisFunction& ephemeris,
                                double correction_seconds) {
  TaggedEvaluation result;
  EphemerisSample moon_sample;
  EphemerisSample body_sample;
  if (!ephemeris(correction_seconds, &result.distance_sample, &result.error) ||
      !ephemeris(correction_seconds + observation.moon_time_offset_seconds,
                 &moon_sample, &result.error) ||
      !ephemeris(correction_seconds + observation.body_time_offset_seconds,
                 &body_sample, &result.error))
    return result;
  double moon_altitude = 0.0, body_altitude = 0.0;
  if (!CorrectObservedAltitude(observation, moon_sample, true, &moon_altitude,
                               &result.error) ||
      !CorrectObservedAltitude(observation, body_sample, false, &body_altitude,
                               &result.error))
    return result;
  result.positions = SolveReferencePositions(
      observation, moon_sample, body_sample, moon_altitude, body_altitude,
      &result.error);
  for (const GeographicPoint& position : result.positions) {
    const double predicted =
        PredictedRawDistance(observation, result.distance_sample, position);
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
      if (std::fabs(augmented[row][pivot]) >
          std::fabs(augmented[best][pivot]))
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
    double correction_seconds, const GeographicPoint& reference_position) {
  TaggedUncertainty result;
  constexpr double time_step_seconds = 30.0;
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

  double before[3], after[3];
  if (!values(correction_seconds - time_step_seconds, reference_position,
              before) ||
      !values(correction_seconds + time_step_seconds, reference_position,
              after))
    return result;
  const double time_step_hours = time_step_seconds / 3600.0;
  for (int observation_index = 0; observation_index < 3;
       ++observation_index)
    jacobian[observation_index][0] =
        (after[observation_index] - before[observation_index]) /
        (2.0 * time_step_hours);

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
    for (int observation_index = 0; observation_index < 3;
         ++observation_index)
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

SolveResult SolveTimeTagged(const Observation& observation,
                            const EphemerisFunction& ephemeris,
                            const SolveOptions& options) {
  SolveResult result;
  if (!ephemeris || !(options.end_offset_seconds > options.start_offset_seconds) ||
      !(options.scan_step_seconds > 0.0) ||
      !(options.root_tolerance_seconds > 0.0)) {
    result.error = "Invalid time-tagged lunar-distance search";
    return result;
  }
  if (!observation.separate_times) {
    result.error = "Separate angle times are not enabled";
    return result;
  }
  if (!Finite(observation.moon_time_offset_seconds) ||
      !Finite(observation.body_time_offset_seconds) ||
      !Finite(observation.course_true_deg) ||
      !Finite(observation.speed_knots) || observation.speed_knots < 0.0) {
    result.error = "A time offset or motion input is invalid";
    return result;
  }
  if (!Finite(observation.raw_distance_deg) ||
      !Finite(observation.moon_altitude_deg) ||
      !Finite(observation.body_altitude_deg) ||
      observation.raw_distance_deg <= 0.0 ||
      observation.raw_distance_deg >= 180.0 ||
      observation.moon_altitude_deg <= -90.0 ||
      observation.moon_altitude_deg >= 180.0 ||
      observation.body_altitude_deg <= -90.0 ||
      observation.body_altitude_deg >= 180.0 ||
      observation.eye_height_m < 0.0 || observation.pressure_hpa <= 0.0 ||
      observation.temperature_c <= -100.0 ||
      observation.distance_uncertainty_arcmin < 0.0 ||
      observation.moon_altitude_uncertainty_arcmin < 0.0 ||
      observation.body_altitude_uncertainty_arcmin < 0.0 ||
      (observation.dip_short &&
       observation.dip_short_distance_m <= 0.0)) {
    result.error = "A time-tagged lunar observation input is outside its usable range";
    return result;
  }

  struct ScanPoint {
    double correction;
    TaggedEvaluation evaluation;
  };
  std::vector<ScanPoint> points;
  double closest = std::numeric_limits<double>::infinity();
  bool found_closest = false;
  for (double correction = options.start_offset_seconds;
       correction < options.end_offset_seconds;
       correction += options.scan_step_seconds) {
    TaggedEvaluation evaluation =
        EvaluateTagged(observation, ephemeris, correction);
    if (evaluation.valid) {
      for (double residual : evaluation.residuals) {
        if (std::fabs(residual) < closest) {
          closest = std::fabs(residual);
          found_closest = true;
          result.closest_offset_seconds = correction;
          result.closest_residual_arcmin = residual * 60.0;
        }
      }
    }
    points.push_back({correction, std::move(evaluation)});
  }
  points.push_back({options.end_offset_seconds,
                    EvaluateTagged(observation, ephemeris,
                                   options.end_offset_seconds)});

  for (std::size_t index = 1; index < points.size(); ++index) {
    if (!points[index - 1].evaluation.valid ||
        !points[index].evaluation.valid)
      continue;
    const std::size_t branches =
        std::min(points[index - 1].evaluation.residuals.size(),
                 points[index].evaluation.residuals.size());
    for (std::size_t branch = 0; branch < branches; ++branch) {
      double left = points[index - 1].correction;
      double right = points[index].correction;
      double fleft = points[index - 1].evaluation.residuals[branch];
      double fright = points[index].evaluation.residuals[branch];
      if (fleft != 0.0 && fright != 0.0 &&
          std::signbit(fleft) == std::signbit(fright))
        continue;
      bool bracket_valid = true;
      for (int iteration = 0;
           iteration < 80 && right - left > options.root_tolerance_seconds;
           ++iteration) {
        const double middle = 0.5 * (left + right);
        TaggedEvaluation evaluation =
            EvaluateTagged(observation, ephemeris, middle);
        if (!evaluation.valid || branch >= evaluation.residuals.size()) {
          bracket_valid = false;
          break;
        }
        const double fmiddle = evaluation.residuals[branch];
        if (fmiddle == 0.0 ||
            std::signbit(fmiddle) == std::signbit(fleft)) {
          left = middle;
          fleft = fmiddle;
        } else {
          right = middle;
          fright = fmiddle;
        }
      }
      if (!bracket_valid) continue;
      const double root = 0.5 * (left + right);
      TaggedEvaluation root_evaluation =
          EvaluateTagged(observation, ephemeris, root);
      if (!root_evaluation.valid || branch >= root_evaluation.positions.size())
        continue;
      const double derivative_interval = 30.0;
      const TaggedEvaluation before = EvaluateTagged(
          observation, ephemeris, root - derivative_interval);
      const TaggedEvaluation after = EvaluateTagged(
          observation, ephemeris, root + derivative_interval);
      if (!before.valid || !after.valid ||
          branch >= before.residuals.size() || branch >= after.residuals.size())
        continue;
      const double slope_arcmin_per_hour =
          (after.residuals[branch] - before.residuals[branch]) * 60.0 *
          3600.0 / (2.0 * derivative_interval);
      const double angular_uncertainty = std::hypot(
          observation.distance_uncertainty_arcmin,
          std::hypot(observation.moon_altitude_uncertainty_arcmin,
                     observation.body_altitude_uncertainty_arcmin));
      const double slope_arcmin_per_second =
          std::fabs(slope_arcmin_per_hour) / 3600.0;

      TimeCandidate candidate;
      candidate.offset_seconds = root;
      candidate.predicted_distance_deg =
          root_evaluation.distance_sample.predicted_distance_deg;
      candidate.cleared_distance_deg =
          root_evaluation.distance_sample.predicted_distance_deg;
      candidate.slope_arcmin_per_hour = slope_arcmin_per_hour;
      candidate.angular_uncertainty_arcmin = angular_uncertainty;
      candidate.positions.push_back(root_evaluation.positions[branch]);
      const TaggedUncertainty uncertainty = EstimateTaggedUncertainty(
          observation, ephemeris, root, candidate.positions.front());
      candidate.time_uncertainty_seconds =
          uncertainty.valid
              ? uncertainty.time_seconds
              : (slope_arcmin_per_second > 1e-12
                     ? angular_uncertainty / slope_arcmin_per_second
                     : std::numeric_limits<double>::infinity());
      candidate.position_uncertainty_nm = uncertainty.position_nm;

      bool merged = false;
      for (TimeCandidate& existing : result.candidates) {
        if (std::fabs(existing.offset_seconds - root) <
            2.0 * options.root_tolerance_seconds) {
          bool duplicate_position = false;
          for (const GeographicPoint& position : existing.positions)
            if (ToDegrees(std::acos(ClampUnit(Dot(
                    Unit(position), Unit(candidate.positions.front()))))) <
                1e-5)
              duplicate_position = true;
          if (!duplicate_position)
            existing.positions.push_back(candidate.positions.front());
          merged = true;
          break;
        }
      }
      if (!merged) result.candidates.push_back(candidate);
    }
  }

  std::sort(result.candidates.begin(), result.candidates.end(),
            [](const TimeCandidate& first, const TimeCandidate& second) {
              return first.offset_seconds < second.offset_seconds;
            });
  if (result.candidates.empty()) {
    std::ostringstream message;
    message << "No joint UTC/position solution occurs in the selected interval";
    if (found_closest)
      message << "; closest lunar-distance residual is "
              << result.closest_residual_arcmin << " arcmin";
    result.error = message.str();
    return result;
  }
  if (result.candidates.size() > 1)
    result.warnings.push_back(
        "More than one joint time/position solution exists; use DR, hemisphere, another altitude or a second lunar to resolve it");
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
  result.raw_distance_deg = PredictedRawDistance(
      settings, distance_sample, reference_position);
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
  const double latitude_difference =
      ToRadians(second.latitude_deg - first.latitude_deg);
  const double longitude_difference =
      ToRadians(second.longitude_deg - first.longitude_deg);
  const double first_latitude = ToRadians(first.latitude_deg);
  const double second_latitude = ToRadians(second.latitude_deg);
  const double haversine =
      std::sin(latitude_difference / 2.0) *
          std::sin(latitude_difference / 2.0) +
      std::cos(first_latitude) * std::cos(second_latitude) *
          std::sin(longitude_difference / 2.0) *
          std::sin(longitude_difference / 2.0);
  return ToDegrees(2.0 * std::atan2(std::sqrt(std::max(0.0, haversine)),
                                    std::sqrt(std::max(0.0, 1.0 - haversine)))) *
         60.0;
}

}  // namespace lunar_distance
