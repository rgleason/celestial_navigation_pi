#include <gtest/gtest.h>

#include "SextantCalibrationEngine.h"

TEST(SextantCalibrationEngine, PredictsSimpleEquatorialSeparation) {
  sextant_calibration::BodySample first;
  first.geographic_latitude_deg = 0.0;
  first.geographic_longitude_deg = -30.0;
  sextant_calibration::BodySample second;
  second.geographic_latitude_deg = 0.0;
  second.geographic_longitude_deg = 30.0;
  sextant_calibration::Environment environment;
  environment.observer = {0.0, 0.0};
  environment.pressure_hpa = 0.0;
  const auto result = sextant_calibration::PredictApparentCenterDistance(
      first, second, environment);
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_NEAR(result.apparent_center_distance_deg, 60.0, 1e-8);
  EXPECT_NEAR(result.apparent_near_contact_distance_deg, 60.0, 1e-8);
  EXPECT_NEAR(result.apparent_far_contact_distance_deg, 60.0, 1e-8);
  EXPECT_NEAR(result.altitude_difference_deg, 0.0, 1e-8);
}

TEST(SextantCalibrationEngine, AppliesBothSemidiametersToContacts) {
  sextant_calibration::BodySample first;
  first.geographic_latitude_deg = 0.0;
  first.geographic_longitude_deg = -30.0;
  first.semidiameter_deg = 0.25;
  sextant_calibration::BodySample second;
  second.geographic_latitude_deg = 0.0;
  second.geographic_longitude_deg = 30.0;
  second.semidiameter_deg = 0.2;
  sextant_calibration::Environment environment;
  environment.observer = {0.0, 0.0};
  environment.pressure_hpa = 0.0;
  const auto result = sextant_calibration::PredictApparentCenterDistance(
      first, second, environment);
  ASSERT_TRUE(result.valid);
  EXPECT_NEAR(result.apparent_near_contact_distance_deg, 59.55, 1e-8);
  EXPECT_NEAR(result.apparent_far_contact_distance_deg, 60.45, 1e-8);
}

TEST(SextantCalibrationEngine, BuildsPiecewiseCorrectionProfile) {
  std::vector<sextant_calibration::CheckReading> readings = {
      {20.0, 20.0 + 1.0 / 60.0, 0.2, "A"},
      {22.0, 22.0 + 1.2 / 60.0, 0.2, "B"},
      {70.0, 70.0 - 2.0 / 60.0, 0.2, "C"},
      {72.0, 72.0 - 1.8 / 60.0, 0.2, "D"}};
  const auto profile = sextant_calibration::BuildProfile(
      "Primary", "123", "2026-08-15T12:00:00Z", readings, 10.0);
  ASSERT_EQ(profile.points.size(), 2u);
  EXPECT_NEAR(profile.points[0].correction_arcmin, -1.1, 1e-10);
  EXPECT_NEAR(profile.points[1].correction_arcmin, 1.9, 1e-10);
  double uncertainty = 0.0;
  EXPECT_NEAR(sextant_calibration::CorrectionAt(profile, 46.5, &uncertainty),
              0.4, 0.1);
  EXPECT_GT(uncertainty, 0.0);
}

TEST(SextantCalibrationEngine, ClampsOutsideMeasuredRange) {
  sextant_calibration::Profile profile;
  profile.points = {{10.0, -0.5, 0.2, 2}, {80.0, 1.0, 0.3, 2}};
  EXPECT_DOUBLE_EQ(sextant_calibration::CorrectionAt(profile, 2.0), -0.5);
  EXPECT_DOUBLE_EQ(sextant_calibration::CorrectionAt(profile, 100.0), 1.0);
}
