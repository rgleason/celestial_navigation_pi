// POBsoft (1985-2026): independent uncertainty examples for Android profiles.
#include <gtest/gtest.h>

#include "SextantCalibrationEngine.h"

namespace {
sextant_calibration::CheckReading Reading(double uncertainty,
                                          double residual = 1.5) {
  sextant_calibration::CheckReading result;
  result.predicted_deg = 23.833854159275464;
  result.index_error_arcmin = 1.5;
  result.observed_deg = result.predicted_deg + (1.5 - residual) / 60.0;
  result.uncertainty_arcmin = uncertainty;
  return result;
}
}

TEST(AndroidSextantUncertainty, EqualIndependentRepeatsRetainMeasurementError) {
  const auto profile = sextant_calibration::BuildProfile(
      "disposable", "test", "", {Reading(0.2), Reading(0.2)});
  ASSERT_EQ(1u, profile.points.size());
  EXPECT_NEAR(0.1414213562373095, profile.points[0].uncertainty_arcmin, 1e-12);
  EXPECT_NEAR(1.5, profile.points[0].correction_arcmin, 1e-10);
  EXPECT_NEAR(23.808854159275464, profile.points[0].angle_deg, 1e-12);
  EXPECT_DOUBLE_EQ(0.0, profile.repeatability_arcmin);
}

TEST(AndroidSextantUncertainty, UnequalIndependentRepeatsUseFormalMeanError) {
  const auto profile = sextant_calibration::BuildProfile(
      "disposable", "test", "", {Reading(0.2), Reading(0.4)});
  ASSERT_EQ(1u, profile.points.size());
  EXPECT_NEAR(0.1788854381999832, profile.points[0].uncertainty_arcmin, 1e-12);
}

TEST(AndroidSextantUncertainty, ScatterCanDominateMeasurementError) {
  const auto profile = sextant_calibration::BuildProfile(
      "disposable", "test", "", {Reading(0.05, -1.0), Reading(0.05, 1.0)});
  ASSERT_EQ(1u, profile.points.size());
  EXPECT_NEAR(1.0, profile.points[0].uncertainty_arcmin, 1e-10);
  EXPECT_NEAR(0.0, profile.points[0].correction_arcmin, 1e-10);
}

TEST(AndroidSextantUncertainty, SingleReadingRetainsEnteredUncertainty) {
  const auto profile = sextant_calibration::BuildProfile(
      "disposable", "test", "", {Reading(0.2)});
  ASSERT_EQ(1u, profile.points.size());
  EXPECT_DOUBLE_EQ(0.2, profile.points[0].uncertainty_arcmin);
}
