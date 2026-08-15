#ifndef CELESTIAL_NAVIGATION_SEXTANT_CALIBRATION_ENGINE_H
#define CELESTIAL_NAVIGATION_SEXTANT_CALIBRATION_ENGINE_H

#include "LunarDistanceEngine.h"

#include <string>
#include <vector>

namespace sextant_calibration {

struct BodySample {
  std::string name;
  double geographic_latitude_deg = 0.0;
  double geographic_longitude_deg = 0.0;
  double horizontal_parallax_deg = 0.0;
  double semidiameter_deg = 0.0;
};

struct Environment {
  lunar_distance::GeographicPoint observer;
  double pressure_hpa = 1013.0;
  double temperature_c = 10.0;
};

struct PairPrediction {
  bool valid = false;
  std::string error;
  double apparent_center_distance_deg = 0.0;
  double apparent_near_contact_distance_deg = 0.0;
  double apparent_far_contact_distance_deg = 0.0;
  double first_altitude_deg = 0.0;
  double second_altitude_deg = 0.0;
  double altitude_difference_deg = 0.0;
};

PairPrediction PredictApparentCenterDistance(const BodySample& first,
                                             const BodySample& second,
                                             const Environment& environment);

struct CheckReading {
  double predicted_deg = 0.0;
  double observed_deg = 0.0;
  double uncertainty_arcmin = 0.2;
  std::string note;
};

struct CorrectionPoint {
  double angle_deg = 0.0;
  // Add this value to a raw sextant reading.
  double correction_arcmin = 0.0;
  double uncertainty_arcmin = 0.0;
  int reading_count = 0;
};

struct Profile {
  std::string name;
  std::string serial_number;
  std::string created_utc;
  std::vector<CorrectionPoint> points;
  double repeatability_arcmin = 0.0;
};

Profile BuildProfile(const std::string& name, const std::string& serial,
                     const std::string& created_utc,
                     const std::vector<CheckReading>& readings,
                     double bin_width_deg = 10.0);

double CorrectionAt(const Profile& profile, double observed_angle_deg,
                    double* uncertainty_arcmin = nullptr);

}  // namespace sextant_calibration

#endif
