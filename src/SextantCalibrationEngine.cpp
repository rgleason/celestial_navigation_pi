#include "SextantCalibrationEngine.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace sextant_calibration {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

double Refraction(double altitude_deg, double pressure_hpa,
                  double temperature_c) {
  if (altitude_deg < -1.0 || altitude_deg > 90.0) return 0.0;
  const double denominator =
      std::tan((altitude_deg + 10.3 / (altitude_deg + 5.11)) * kDegToRad);
  if (std::fabs(denominator) < 1e-9) return 0.0;
  return (1.02 / denominator) / 60.0 * (pressure_hpa / 1010.0) *
         (283.0 / (273.0 + temperature_c));
}

struct Horizontal {
  double altitude = 0.0;
  double azimuth = 0.0;
  Horizontal() {}
  Horizontal(double altitude_value, double azimuth_value)
      : altitude(altitude_value), azimuth(azimuth_value) {}
};

Horizontal ToHorizontal(const BodySample& body,
                        const Environment& environment) {
  const double latitude = environment.observer.latitude_deg * kDegToRad;
  const double declination = body.geographic_latitude_deg * kDegToRad;
  const double hour_angle =
      (body.geographic_longitude_deg + environment.observer.longitude_deg) *
      kDegToRad;
  const double sin_altitude =
      std::sin(latitude) * std::sin(declination) +
      std::cos(latitude) * std::cos(declination) * std::cos(hour_angle);
  const double geocentric_altitude =
      std::asin(std::max(-1.0, std::min(1.0, sin_altitude))) / kDegToRad;
  const double azimuth =
      std::atan2(std::sin(hour_angle),
                 std::cos(hour_angle) * std::sin(latitude) -
                     std::tan(declination) * std::cos(latitude)) /
          kDegToRad +
      180.0;
  const double topocentric =
      geocentric_altitude -
      body.horizontal_parallax_deg * std::cos(geocentric_altitude * kDegToRad);
  return {topocentric + Refraction(topocentric, environment.pressure_hpa,
                                   environment.temperature_c),
          azimuth};
}

}  // namespace

PairPrediction PredictApparentCenterDistance(const BodySample& first,
                                             const BodySample& second,
                                             const Environment& environment) {
  PairPrediction result;
  const Horizontal a = ToHorizontal(first, environment);
  const Horizontal b = ToHorizontal(second, environment);
  if (a.altitude <= -1.0 || b.altitude <= -1.0) {
    result.error = "One or both bodies are below the usable horizon";
    return result;
  }
  const double cosine =
      std::sin(a.altitude * kDegToRad) * std::sin(b.altitude * kDegToRad) +
      std::cos(a.altitude * kDegToRad) * std::cos(b.altitude * kDegToRad) *
          std::cos((a.azimuth - b.azimuth) * kDegToRad);
  result.apparent_center_distance_deg =
      std::acos(std::max(-1.0, std::min(1.0, cosine))) / kDegToRad;
  // Meeus' topocentric semidiameter factor matters for the nearby Moon and
  // is small for the other bodies.  Contact predictions must use the
  // apparent limb at the observer, not a geocentric disk pasted onto a
  // topocentric centre direction.
  const double first_semidiameter =
      first.semidiameter_deg *
      (1.0 + std::sin(a.altitude * kDegToRad) *
                 std::sin(first.horizontal_parallax_deg * kDegToRad));
  const double second_semidiameter =
      second.semidiameter_deg *
      (1.0 + std::sin(b.altitude * kDegToRad) *
                 std::sin(second.horizontal_parallax_deg * kDegToRad));
  const double semidiameters = first_semidiameter + second_semidiameter;
  result.apparent_near_contact_distance_deg =
      std::max(0.0, result.apparent_center_distance_deg - semidiameters);
  result.apparent_far_contact_distance_deg =
      std::min(180.0, result.apparent_center_distance_deg + semidiameters);
  result.first_altitude_deg = a.altitude;
  result.second_altitude_deg = b.altitude;
  result.altitude_difference_deg = std::fabs(a.altitude - b.altitude);
  result.valid = std::isfinite(result.apparent_center_distance_deg);
  if (!result.valid) result.error = "The apparent separation is not finite";
  return result;
}

Profile BuildProfile(const std::string& name, const std::string& serial,
                     const std::string& created_utc,
                     const std::vector<CheckReading>& readings,
                     double bin_width_deg) {
  Profile profile;
  profile.name = name;
  profile.serial_number = serial;
  profile.created_utc = created_utc;
  bin_width_deg = std::max(1.0, bin_width_deg);
  std::map<int, std::vector<const CheckReading*>> bins;
  std::vector<double> all_corrections;
  for (const auto& reading : readings) {
    if (!std::isfinite(reading.predicted_deg) ||
        !std::isfinite(reading.observed_deg))
      continue;
    // Bin on the computed true angle, not the imperfect instrument reading;
    // otherwise a small negative error exactly on a bin boundary can split a
    // repeat set into two artificial calibration ranges.
    bins[static_cast<int>(std::floor(reading.predicted_deg / bin_width_deg))]
        .push_back(&reading);
    all_corrections.push_back((reading.predicted_deg - reading.observed_deg) *
                              60.0);
  }
  for (const auto& bin : bins) {
    double weighted_angle = 0.0;
    double weighted_correction = 0.0;
    double weight_sum = 0.0;
    for (const CheckReading* reading : bin.second) {
      const double sigma = std::max(0.05, reading->uncertainty_arcmin);
      const double weight = 1.0 / (sigma * sigma);
      weighted_angle += weight * reading->observed_deg;
      weighted_correction +=
          weight * (reading->predicted_deg - reading->observed_deg) * 60.0;
      weight_sum += weight;
    }
    CorrectionPoint point;
    point.angle_deg = weighted_angle / weight_sum;
    point.correction_arcmin = weighted_correction / weight_sum;
    point.reading_count = static_cast<int>(bin.second.size());
    double scatter = 0.0;
    for (const CheckReading* reading : bin.second) {
      const double correction =
          (reading->predicted_deg - reading->observed_deg) * 60.0;
      scatter += (correction - point.correction_arcmin) *
                 (correction - point.correction_arcmin);
    }
    point.uncertainty_arcmin =
        bin.second.size() > 1
            ? std::sqrt(scatter / (bin.second.size() - 1)) /
                  std::sqrt(static_cast<double>(bin.second.size()))
            : bin.second.front()->uncertainty_arcmin;
    profile.points.push_back(point);
  }
  if (all_corrections.size() > 1) {
    double mean = 0.0;
    for (double value : all_corrections) mean += value;
    mean /= all_corrections.size();
    double variance = 0.0;
    for (double value : all_corrections)
      variance += (value - mean) * (value - mean);
    profile.repeatability_arcmin =
        std::sqrt(variance / (all_corrections.size() - 1));
  }
  return profile;
}

double CorrectionAt(const Profile& profile, double angle,
                    double* uncertainty_arcmin) {
  if (profile.points.empty()) {
    if (uncertainty_arcmin) *uncertainty_arcmin = 0.0;
    return 0.0;
  }
  if (angle <= profile.points.front().angle_deg) {
    if (uncertainty_arcmin)
      *uncertainty_arcmin = profile.points.front().uncertainty_arcmin;
    return profile.points.front().correction_arcmin;
  }
  if (angle >= profile.points.back().angle_deg) {
    if (uncertainty_arcmin)
      *uncertainty_arcmin = profile.points.back().uncertainty_arcmin;
    return profile.points.back().correction_arcmin;
  }
  for (std::size_t index = 1; index < profile.points.size(); ++index) {
    if (angle > profile.points[index].angle_deg) continue;
    const auto& low = profile.points[index - 1];
    const auto& high = profile.points[index];
    const double fraction =
        (angle - low.angle_deg) / (high.angle_deg - low.angle_deg);
    if (uncertainty_arcmin)
      *uncertainty_arcmin =
          low.uncertainty_arcmin +
          fraction * (high.uncertainty_arcmin - low.uncertainty_arcmin);
    return low.correction_arcmin +
           fraction * (high.correction_arcmin - low.correction_arcmin);
  }
  return profile.points.back().correction_arcmin;
}

}  // namespace sextant_calibration
