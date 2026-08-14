#include <gtest/gtest.h>

#include "CoastalNavigationEngine.h"

#include <cmath>

namespace cn = coastal_navigation;

TEST(CoastalNavigation, WaterlineTopRangeIncludesEyeHeightAndCurvature) {
  cn::VerticalAngleObservation observation;
  observation.mode = cn::VerticalAngleMode::WaterlineToTop;
  observation.angle_deg = 1.0;
  observation.charted_top_height_m = 30.0;
  observation.eye_height_m = 2.5;
  observation.terrestrial_refraction_coefficient = 0.13;
  const cn::RangeResult result = cn::SolveVerticalAngle(observation);
  ASSERT_TRUE(result.valid) << result.error;
  const double simple_nm = 30.0 / std::tan(M_PI / 180.0) / 1852.0;
  EXPECT_NEAR(result.range_nm, simple_nm, 0.08);
}

TEST(CoastalNavigation, TideChangesEffectiveTargetHeightAndRange) {
  cn::VerticalAngleObservation low_water;
  low_water.angle_deg = 0.5;
  low_water.charted_top_height_m = 40.0;
  low_water.eye_height_m = 2.0;
  cn::VerticalAngleObservation high_water = low_water;
  high_water.water_level_above_height_datum_m = 5.0;
  const cn::RangeResult low = cn::SolveVerticalAngle(low_water);
  const cn::RangeResult high = cn::SolveVerticalAngle(high_water);
  ASSERT_TRUE(low.valid) << low.error;
  ASSERT_TRUE(high.valid) << high.error;
  EXPECT_GT(low.range_nm, high.range_nm);
}

TEST(CoastalNavigation, BowditchBeyondHorizonMatchesTableFormula) {
  cn::VerticalAngleObservation observation;
  observation.mode =
      cn::VerticalAngleMode::SeaHorizonToTopBeyondHorizon;
  observation.angle_deg = 10.0 / 60.0;
  observation.charted_top_height_m = 100.0 * 0.3048;
  observation.eye_height_m = 0.0;
  const cn::RangeResult result = cn::SolveVerticalAngle(observation);
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_NEAR(result.range_nm, 4.7, 0.1);
}

TEST(CoastalNavigation, ThreePointHorizontalFixRecoversKnownPosition) {
  cn::HorizontalAngleObservation observation;
  observation.left = {50.02, -1.05};
  observation.centre = {50.08, -0.98};
  observation.right = {50.00, -0.90};
  const cn::GeoPoint truth{49.95, -1.00};
  observation.left_centre_angle_deg = cn::IncludedHorizontalAngleDeg(
      truth, observation.left, observation.centre);
  observation.centre_right_angle_deg = cn::IncludedHorizontalAngleDeg(
      truth, observation.centre, observation.right);
  const cn::HorizontalFixResult result = cn::SolveHorizontalThreePointFix(
      observation, cn::GeoPoint{49.96, -1.01});
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_LT(cn::GreatCircleDistanceNm(result.position, truth), 0.001);
  EXPECT_LT(std::fabs(result.first_residual_arcmin), 0.001);
  EXPECT_LT(std::fabs(result.second_residual_arcmin), 0.001);
}

TEST(CoastalNavigation, SingleHorizontalAngleProducesChartableLocus) {
  const cn::GeoPoint first{53.0, -3.1};
  const cn::GeoPoint second{53.0, -2.9};
  const auto branches =
      cn::BuildHorizontalAngleLocus(first, second, 40.0, 720);
  ASSERT_FALSE(branches.empty());
  std::size_t points = 0;
  for (const auto& branch : branches) {
    points += branch.size();
    for (const auto& point : branch)
      EXPECT_NEAR(cn::IncludedHorizontalAngleDeg(point, first, second), 40.0,
                  0.5);
  }
  EXPECT_GT(points, 20u);
}
