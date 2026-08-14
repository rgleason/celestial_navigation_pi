#ifndef CELESTIAL_NAVIGATION_LUNAR_DISTANCE_ENGINE_H
#define CELESTIAL_NAVIGATION_LUNAR_DISTANCE_ENGINE_H

#include <functional>
#include <string>
#include <vector>

namespace lunar_distance {

enum class AltitudeLimb { Lower, Center, Upper };
enum class DistanceContact { Near, Center, Far };

struct Observation {
  double raw_distance_deg = 0.0;
  double moon_altitude_deg = 0.0;
  double body_altitude_deg = 0.0;
  AltitudeLimb moon_altitude_limb = AltitudeLimb::Lower;
  AltitudeLimb body_altitude_limb = AltitudeLimb::Center;
  DistanceContact moon_contact = DistanceContact::Near;
  DistanceContact body_contact = DistanceContact::Center;
  double index_error_arcmin = 0.0;
  double eye_height_m = 2.0;
  double pressure_hpa = 1013.0;
  double temperature_c = 10.0;
  bool artificial_horizon = false;
  bool dip_short = false;
  double dip_short_distance_m = 0.0;
  double distance_uncertainty_arcmin = 0.2;
  double moon_altitude_uncertainty_arcmin = 0.2;
  double body_altitude_uncertainty_arcmin = 0.2;
};

struct EphemerisSample {
  double predicted_distance_deg = 0.0;
  double moon_semidiameter_deg = 0.0;
  double moon_horizontal_parallax_deg = 0.0;
  double body_semidiameter_deg = 0.0;
  double body_horizontal_parallax_deg = 0.0;
  double moon_geographic_latitude_deg = 0.0;
  double moon_geographic_longitude_deg = 0.0;
  double body_geographic_latitude_deg = 0.0;
  double body_geographic_longitude_deg = 0.0;
};

struct GeographicPoint {
  double latitude_deg;
  double longitude_deg;
  GeographicPoint(double latitude = 0.0, double longitude = 0.0)
      : latitude_deg(latitude), longitude_deg(longitude) {}
};

struct PositionResult {
  bool valid = false;
  std::string error;
  std::vector<GeographicPoint> candidates;
  double circle_crossing_angle_deg = 0.0;
};

struct Clearance {
  bool valid = false;
  std::string error;
  double apparent_distance_deg = 0.0;
  double cleared_distance_deg = 0.0;
  double moon_apparent_center_altitude_deg = 0.0;
  double body_apparent_center_altitude_deg = 0.0;
  double moon_geocentric_altitude_deg = 0.0;
  double body_geocentric_altitude_deg = 0.0;
  double relative_azimuth_deg = 0.0;
};

struct TimeCandidate {
  double offset_seconds = 0.0;
  double cleared_distance_deg = 0.0;
  double predicted_distance_deg = 0.0;
  double slope_arcmin_per_hour = 0.0;
  double angular_uncertainty_arcmin = 0.0;
  double time_uncertainty_seconds = 0.0;
};

struct SolveOptions {
  double start_offset_seconds = -43200.0;
  double end_offset_seconds = 43200.0;
  double scan_step_seconds = 300.0;
  double root_tolerance_seconds = 0.05;
};

struct SolveResult {
  bool valid = false;
  std::string error;
  std::vector<std::string> warnings;
  std::vector<TimeCandidate> candidates;
  double closest_offset_seconds = 0.0;
  double closest_residual_arcmin = 0.0;
};

using EphemerisFunction =
    std::function<bool(double offset_seconds, EphemerisSample* sample,
                       std::string* error)>;

Clearance ClearDistance(const Observation& observation,
                        const EphemerisSample& ephemeris);

SolveResult SolveTime(const Observation& observation,
                      const EphemerisFunction& ephemeris,
                      const SolveOptions& options);

PositionResult IntersectAltitudeCircles(
    const GeographicPoint& moon_geographic_position,
    double moon_observed_altitude_deg,
    const GeographicPoint& body_geographic_position,
    double body_observed_altitude_deg);

double GreatCircleDistanceNm(const GeographicPoint& first,
                             const GeographicPoint& second);

}  // namespace lunar_distance

#endif
