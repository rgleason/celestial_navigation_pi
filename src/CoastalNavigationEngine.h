#ifndef CELESTIAL_NAVIGATION_COASTAL_NAVIGATION_ENGINE_H
#define CELESTIAL_NAVIGATION_COASTAL_NAVIGATION_ENGINE_H

#include <string>
#include <vector>

namespace coastal_navigation {

struct GeoPoint {
  double latitude_deg;
  double longitude_deg;
  GeoPoint(double latitude = 0.0, double longitude = 0.0)
      : latitude_deg(latitude), longitude_deg(longitude) {}
};

enum class VerticalAngleMode {
  WaterlineToTop,
  SeaHorizonToTopBeyondHorizon
};

struct VerticalAngleObservation {
  VerticalAngleMode mode = VerticalAngleMode::WaterlineToTop;
  double angle_deg = 0.0;
  double index_error_arcmin = 0.0;
  double charted_top_height_m = 0.0;
  double water_level_above_height_datum_m = 0.0;
  double eye_height_m = 2.0;
  double terrestrial_refraction_coefficient = 0.13;
};

struct RangeResult {
  bool valid = false;
  std::string error;
  std::vector<std::string> warnings;
  double corrected_angle_deg = 0.0;
  double effective_height_m = 0.0;
  double range_nm = 0.0;
};

struct HorizontalAngleObservation {
  GeoPoint left;
  GeoPoint centre;
  GeoPoint right;
  double left_centre_angle_deg = 0.0;
  double centre_right_angle_deg = 0.0;
  double angle_uncertainty_arcmin = 0.2;
  bool moving_observer = false;
  double first_time_offset_seconds = 0.0;
  double second_time_offset_seconds = 0.0;
  double course_true_deg = 0.0;
  double speed_knots = 0.0;
};

struct HorizontalFixResult {
  bool valid = false;
  std::string error;
  GeoPoint position;
  double first_residual_arcmin = 0.0;
  double second_residual_arcmin = 0.0;
  double estimated_uncertainty_nm = 0.0;
  double geometry_condition = 0.0;
};

RangeResult SolveVerticalAngle(const VerticalAngleObservation& observation);

double GreatCircleDistanceNm(const GeoPoint& first, const GeoPoint& second);
double InitialBearingDeg(const GeoPoint& from, const GeoPoint& to);
double IncludedHorizontalAngleDeg(const GeoPoint& observer,
                                  const GeoPoint& first,
                                  const GeoPoint& second);
GeoPoint Destination(const GeoPoint& start, double bearing_deg,
                     double distance_nm);

HorizontalFixResult SolveHorizontalThreePointFix(
    const HorizontalAngleObservation& observation,
    const GeoPoint& initial_position);

// A charting aid for a single HSA. The returned branches use a local tangent
// construction and are intended for coastal-scale display; the numerical fix
// above evaluates exact spherical bearings.
std::vector<std::vector<GeoPoint>> BuildHorizontalAngleLocus(
    const GeoPoint& first, const GeoPoint& second, double angle_deg,
    int samples_per_circle = 720);

}  // namespace coastal_navigation

#endif
