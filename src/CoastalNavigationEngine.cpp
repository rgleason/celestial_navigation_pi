#include "CoastalNavigationEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace coastal_navigation {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kEarthRadiusM = 6371008.8;
constexpr double kMetresPerNm = 1852.0;

double Rad(double degrees) { return degrees * kPi / 180.0; }
double Deg(double radians) { return radians * 180.0 / kPi; }
double Clamp(double value, double low, double high) {
  return std::max(low, std::min(high, value));
}
double NormalizeLongitude(double longitude) {
  longitude = std::fmod(longitude + 180.0, 360.0);
  if (longitude < 0.0) longitude += 360.0;
  return longitude - 180.0;
}
double NormalizeBearing(double bearing) {
  bearing = std::fmod(bearing, 360.0);
  return bearing < 0.0 ? bearing + 360.0 : bearing;
}

double VerticalSubtendedAngle(double range_m, double earth_radius_m,
                              double eye_height_m, double top_height_m) {
  const double sigma = range_m / earth_radius_m;
  const double observer_x = earth_radius_m + eye_height_m;
  const double base_x = earth_radius_m * std::cos(sigma);
  const double base_y = earth_radius_m * std::sin(sigma);
  const double top_x = (earth_radius_m + top_height_m) * std::cos(sigma);
  const double top_y = (earth_radius_m + top_height_m) * std::sin(sigma);
  const double bx = base_x - observer_x;
  const double by = base_y;
  const double tx = top_x - observer_x;
  const double ty = top_y;
  const double denominator = std::hypot(bx, by) * std::hypot(tx, ty);
  if (!(denominator > 0.0)) return std::numeric_limits<double>::quiet_NaN();
  return Deg(std::acos(Clamp((bx * tx + by * ty) / denominator, -1.0, 1.0)));
}

bool ValidPoint(const GeoPoint& point) {
  return std::isfinite(point.latitude_deg) &&
         std::isfinite(point.longitude_deg) &&
         point.latitude_deg > -90.0 && point.latitude_deg < 90.0;
}

}  // namespace

RangeResult SolveVerticalAngle(const VerticalAngleObservation& observation) {
  RangeResult result;
  const double corrected =
      observation.angle_deg - observation.index_error_arcmin / 60.0 -
      (observation.mode == VerticalAngleMode::SeaHorizonToTopBeyondHorizon
           ? 1.758 * std::sqrt(std::max(0.0, observation.eye_height_m)) / 60.0
           : 0.0);
  result.corrected_angle_deg = corrected;
  result.effective_height_m = observation.charted_top_height_m -
                              observation.water_level_above_height_datum_m;
  if (!std::isfinite(corrected) || corrected <= 0.0 || corrected >= 90.0) {
    result.error = "Corrected vertical angle must be between 0 and 90 degrees";
    return result;
  }
  if (!(result.effective_height_m > 0.0) || observation.eye_height_m < 0.0) {
    result.error =
        "The target top must be above the current water level and eye height cannot be negative";
    return result;
  }

  if (observation.mode == VerticalAngleMode::SeaHorizonToTopBeyondHorizon) {
    // American Practical Navigator (Bowditch), Table 15. Heights are feet,
    // range is nautical miles; constants include standard terrestrial
    // refraction. The observed angle must already be corrected for IE and dip.
    const double height_difference_ft =
        (result.effective_height_m - observation.eye_height_m) /
        0.3048;
    if (!(height_difference_ft > 0.0)) {
      result.error = "Target top must be above the observer for this method";
      return result;
    }
    const double term = std::tan(Rad(corrected)) / 0.0002419;
    const double radicand = term * term + height_difference_ft / 0.7349;
    if (!(radicand > 0.0)) {
      result.error = "Bowditch distance formula has no real solution";
      return result;
    }
    result.range_nm = std::sqrt(radicand) - term;
    result.warnings.push_back(
        "This Bowditch mode assumes standard terrestrial refraction; unusual atmospheric gradients can move the visible horizon");
  } else {
    const double coefficient = observation.terrestrial_refraction_coefficient;
    if (!std::isfinite(coefficient) || coefficient < 0.0 || coefficient >= 0.5) {
      result.error = "Terrestrial refraction coefficient must be between 0 and 0.5";
      return result;
    }
    const double effective_radius = kEarthRadiusM / (1.0 - coefficient);
    const double visible_horizon_m =
        std::sqrt(2.0 * effective_radius * observation.eye_height_m +
                  observation.eye_height_m * observation.eye_height_m);
    double low = 0.01;
    double high = std::max(1.0, visible_horizon_m * 0.999999);
    const double high_angle = VerticalSubtendedAngle(
        high, effective_radius, observation.eye_height_m,
        result.effective_height_m);
    if (!std::isfinite(high_angle) || corrected < high_angle) {
      result.error =
          "The waterline would be below the visible horizon at the range implied by this angle; use the sea-horizon method instead";
      return result;
    }
    for (int iteration = 0; iteration < 100; ++iteration) {
      const double middle = 0.5 * (low + high);
      const double angle = VerticalSubtendedAngle(
          middle, effective_radius, observation.eye_height_m,
          result.effective_height_m);
      if (angle > corrected)
        low = middle;
      else
        high = middle;
    }
    result.range_nm = 0.5 * (low + high) / kMetresPerNm;
    const double plane_range_nm =
        result.effective_height_m / std::tan(Rad(corrected)) / kMetresPerNm;
    if (result.range_nm > 0.0 &&
        std::fabs(result.range_nm - plane_range_nm) / result.range_nm > 0.03)
      result.warnings.push_back(
          "Curvature, eye height and standard terrestrial refraction change the result by more than 3% from the simple height/tangent estimate");
  }
  result.valid = std::isfinite(result.range_nm) && result.range_nm > 0.0;
  if (!result.valid) result.error = "Vertical-angle range is not finite";
  return result;
}

double GreatCircleDistanceNm(const GeoPoint& first, const GeoPoint& second) {
  const double lat1 = Rad(first.latitude_deg);
  const double lat2 = Rad(second.latitude_deg);
  const double dlat = lat2 - lat1;
  const double dlon = Rad(second.longitude_deg - first.longitude_deg);
  const double a = std::sin(dlat / 2.0) * std::sin(dlat / 2.0) +
                   std::cos(lat1) * std::cos(lat2) *
                       std::sin(dlon / 2.0) * std::sin(dlon / 2.0);
  return 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a))) *
         kEarthRadiusM / kMetresPerNm;
}

double InitialBearingDeg(const GeoPoint& from, const GeoPoint& to) {
  const double lat1 = Rad(from.latitude_deg);
  const double lat2 = Rad(to.latitude_deg);
  const double dlon = Rad(to.longitude_deg - from.longitude_deg);
  const double y = std::sin(dlon) * std::cos(lat2);
  const double x = std::cos(lat1) * std::sin(lat2) -
                   std::sin(lat1) * std::cos(lat2) * std::cos(dlon);
  return NormalizeBearing(Deg(std::atan2(y, x)));
}

double IncludedHorizontalAngleDeg(const GeoPoint& observer,
                                  const GeoPoint& first,
                                  const GeoPoint& second) {
  double difference = std::fabs(InitialBearingDeg(observer, first) -
                                InitialBearingDeg(observer, second));
  if (difference > 180.0) difference = 360.0 - difference;
  return difference;
}

GeoPoint Destination(const GeoPoint& start, double bearing_deg,
                     double distance_nm) {
  const double angular = distance_nm * kMetresPerNm / kEarthRadiusM;
  const double bearing = Rad(bearing_deg);
  const double lat1 = Rad(start.latitude_deg);
  const double lon1 = Rad(start.longitude_deg);
  GeoPoint result;
  const double lat2 = std::asin(Clamp(
      std::sin(lat1) * std::cos(angular) +
          std::cos(lat1) * std::sin(angular) * std::cos(bearing),
      -1.0, 1.0));
  const double lon2 =
      lon1 + std::atan2(std::sin(bearing) * std::sin(angular) * std::cos(lat1),
                        std::cos(angular) - std::sin(lat1) * std::sin(lat2));
  result.latitude_deg = Deg(lat2);
  result.longitude_deg = NormalizeLongitude(Deg(lon2));
  return result;
}

HorizontalFixResult SolveHorizontalThreePointFix(
    const HorizontalAngleObservation& observation,
    const GeoPoint& initial_position) {
  HorizontalFixResult result;
  if (!ValidPoint(observation.left) || !ValidPoint(observation.centre) ||
      !ValidPoint(observation.right) || !ValidPoint(initial_position)) {
    result.error = "A target or initial position is invalid";
    return result;
  }
  if (!(observation.left_centre_angle_deg > 0.0 &&
        observation.left_centre_angle_deg < 180.0 &&
        observation.centre_right_angle_deg > 0.0 &&
        observation.centre_right_angle_deg < 180.0)) {
    result.error = "Horizontal sextant angles must be between 0 and 180 degrees";
    return result;
  }
  if (!std::isfinite(observation.first_time_offset_seconds) ||
      !std::isfinite(observation.second_time_offset_seconds) ||
      !std::isfinite(observation.course_true_deg) ||
      !std::isfinite(observation.speed_knots) || observation.speed_knots < 0.0) {
    result.error = "A horizontal-angle time or motion input is invalid";
    return result;
  }

  auto observer_at = [&](const GeoPoint& reference, double seconds) {
    if (!observation.moving_observer || observation.speed_knots == 0.0 ||
        seconds == 0.0)
      return reference;
    double distance = observation.speed_knots * seconds / 3600.0;
    double course = observation.course_true_deg;
    if (distance < 0.0) {
      distance = -distance;
      course += 180.0;
    }
    return Destination(reference, course, distance);
  };
  auto residuals = [&](const GeoPoint& reference, double* first,
                       double* second) {
    const GeoPoint first_observer =
        observer_at(reference, observation.first_time_offset_seconds);
    const GeoPoint second_observer =
        observer_at(reference, observation.second_time_offset_seconds);
    *first = IncludedHorizontalAngleDeg(first_observer, observation.left,
                                        observation.centre) -
             observation.left_centre_angle_deg;
    *second = IncludedHorizontalAngleDeg(second_observer, observation.centre,
                                         observation.right) -
              observation.centre_right_angle_deg;
  };

  GeoPoint position = initial_position;
  double determinant = 0.0;
  double j00 = 0.0, j01 = 0.0, j10 = 0.0, j11 = 0.0;
  for (int iteration = 0; iteration < 50; ++iteration) {
    double f0 = 0.0, f1 = 0.0;
    residuals(position, &f0, &f1);
    if (std::hypot(f0, f1) * 60.0 < 1e-5) break;
    const double step = 1e-5;
    GeoPoint lat_step = position;
    lat_step.latitude_deg += step;
    GeoPoint lon_step = position;
    lon_step.longitude_deg += step;
    double lat_f0 = 0.0, lat_f1 = 0.0, lon_f0 = 0.0, lon_f1 = 0.0;
    residuals(lat_step, &lat_f0, &lat_f1);
    residuals(lon_step, &lon_f0, &lon_f1);
    j00 = (lat_f0 - f0) / step;
    j10 = (lat_f1 - f1) / step;
    j01 = (lon_f0 - f0) / step;
    j11 = (lon_f1 - f1) / step;
    determinant = j00 * j11 - j01 * j10;
    if (std::fabs(determinant) < 1e-10) {
      result.error = "Horizontal-angle geometry is singular or nearly tangent";
      return result;
    }
    double dlat = (-f0 * j11 + j01 * f1) / determinant;
    double dlon = (-j00 * f1 + f0 * j10) / determinant;
    const double scale = std::max(1.0, std::max(std::fabs(dlat), std::fabs(dlon)) /
                                          0.25);
    dlat /= scale;
    dlon /= scale;
    position.latitude_deg += dlat;
    position.longitude_deg = NormalizeLongitude(position.longitude_deg + dlon);
    if (!ValidPoint(position)) {
      result.error = "Horizontal-angle iteration left the valid chart domain";
      return result;
    }
  }

  double final_first = 0.0, final_second = 0.0;
  residuals(position, &final_first, &final_second);
  result.first_residual_arcmin = final_first * 60.0;
  result.second_residual_arcmin = final_second * 60.0;
  if (std::hypot(result.first_residual_arcmin,
                 result.second_residual_arcmin) > 0.01) {
    result.error = "Horizontal-angle solution did not converge from the supplied approximate position";
    return result;
  }
  // Re-evaluate the Jacobian at the converged fix.  In particular, an exact
  // synthetic observation (or an excellent initial position) may converge
  // before the iteration loop ever needs to form a Jacobian.
  const double step = 1e-5;
  GeoPoint lat_step = position;
  lat_step.latitude_deg += step;
  GeoPoint lon_step = position;
  lon_step.longitude_deg += step;
  double lat_f0 = 0.0, lat_f1 = 0.0, lon_f0 = 0.0, lon_f1 = 0.0;
  residuals(lat_step, &lat_f0, &lat_f1);
  residuals(lon_step, &lon_f0, &lon_f1);
  j00 = (lat_f0 - final_first) / step;
  j10 = (lat_f1 - final_second) / step;
  j01 = (lon_f0 - final_first) / step;
  j11 = (lon_f1 - final_second) / step;
  determinant = j00 * j11 - j01 * j10;
  if (std::fabs(determinant) < 1e-10) {
    result.error = "Horizontal-angle geometry is singular or nearly tangent";
    return result;
  }
  result.position = position;
  const double frobenius = std::sqrt(j00 * j00 + j01 * j01 +
                                     j10 * j10 + j11 * j11);
  result.geometry_condition =
      std::fabs(determinant) > 0.0 ? frobenius * frobenius / std::fabs(determinant)
                                  : std::numeric_limits<double>::infinity();
  const double sigma_deg = observation.angle_uncertainty_arcmin / 60.0;
  const double lat_sigma_deg =
      sigma_deg * std::hypot(j11, j01) / std::fabs(determinant);
  const double lon_sigma_deg =
      sigma_deg * std::hypot(j10, j00) / std::fabs(determinant);
  result.estimated_uncertainty_nm =
      60.0 * std::hypot(lat_sigma_deg,
                        lon_sigma_deg * std::cos(Rad(position.latitude_deg)));
  result.valid = true;
  return result;
}

std::vector<std::vector<GeoPoint>> BuildHorizontalAngleLocus(
    const GeoPoint& first, const GeoPoint& second, double angle_deg,
    int samples_per_circle) {
  std::vector<std::vector<GeoPoint>> result;
  if (!ValidPoint(first) || !ValidPoint(second) || angle_deg <= 0.0 ||
      angle_deg >= 180.0 || samples_per_circle < 36)
    return result;
  const double baseline_nm = GreatCircleDistanceNm(first, second);
  if (!(baseline_nm > 0.0) || baseline_nm > 300.0) return result;
  const double midpoint_bearing = InitialBearingDeg(first, second);
  const GeoPoint midpoint = Destination(first, midpoint_bearing, baseline_nm / 2.0);
  const double radius_nm = baseline_nm / (2.0 * std::sin(Rad(angle_deg)));
  const double centre_offset_nm =
      baseline_nm / (2.0 * std::tan(Rad(angle_deg)));
  for (double side : {-1.0, 1.0}) {
    const GeoPoint centre = Destination(
        midpoint, midpoint_bearing + side * 90.0, std::fabs(centre_offset_nm));
    std::vector<GeoPoint> branch;
    for (int index = 0; index <= samples_per_circle; ++index) {
      const GeoPoint point = Destination(
          centre, index * 360.0 / samples_per_circle, radius_nm);
      if (std::fabs(IncludedHorizontalAngleDeg(point, first, second) -
                    angle_deg) < 0.5)
        branch.push_back(point);
      else if (branch.size() > 2) {
        result.push_back(branch);
        branch.clear();
      } else {
        branch.clear();
      }
    }
    if (branch.size() > 2) result.push_back(branch);
  }
  return result;
}

}  // namespace coastal_navigation
