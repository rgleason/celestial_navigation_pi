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
  // Raw angle read from the sextant.  Keep this untouched for provenance.
  double observed_deg = 0.0;
  double uncertainty_arcmin = 0.2;
  std::string note;
  // Measured index error, using the plugin convention "on the arc +".
  // The index-corrected apparent angle is observed - index error.
  double index_error_arcmin = 0.0;
};

double IndexCorrectedObservedDegrees(const CheckReading& reading);
double ResidualCorrectionArcmin(const CheckReading& reading);

struct CorrectionPoint {
  double angle_deg = 0.0;
  // Add this value after applying the independently measured index error.
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
  // False identifies profiles written by versions which folded index error
  // into a total correction to the raw reading.  Never reinterpret those
  // saved values as the new residual correction.
  bool excludes_index_error = false;
};

Profile BuildProfile(const std::string& name, const std::string& serial,
                     const std::string& created_utc,
                     const std::vector<CheckReading>& readings,
                     double bin_width_deg = 10.0);

double CorrectionAt(const Profile& profile, double observed_angle_deg,
                    double* uncertainty_arcmin = nullptr);

}  // namespace sextant_calibration

#endif
