#include <gtest/gtest.h>

#include "CoastalNavigationEngine.h"

#include <cmath>
#include <array>

namespace cn = coastal_navigation;

TEST(CoastalNavigation, BobRevisionOneVerticalScreenshots) {
  // Revised PDF pp. 3, 4, 6, 7 and 10. Compare the actual screenshot inputs,
  // not rounded table lookup inputs or the conflicting Anvil waypoint height.
  struct Example {
    double height, hs_minutes, target_lat, target_lon, bearing_true;
    double range, fix_lat, fix_lon;
    cn::VerticalAngleMode mode;
  };
  const auto sea=cn::VerticalAngleMode::SeaHorizonToTopBeyondHorizon;
  const auto water=cn::VerticalAngleMode::WaterlineToTop;
  const Example examples[] = {
    {24,45.6,50+39.7/60,-(1+35.5/60),75.22,.974,50+39.4516/60,-(1+36.9841/60),water},
    {24,2.9,50+39.7/60,-(1+35.5/60),65.22,9.676,50+35.6333/60,-(1+49.3301/60),sea},
    {24,4.1,50+39.7/60,-(1+35.5/60),65.22,8.341,50+36.1961/60,-(1+47.4242/60),sea},
    {9,2.9,50+35.5133/60,-(1+57.5883/60),270.25,5.169,50+35.4860/60,-(1+49.4510/60),sea},
    {216,14.6,43+27.6/60,-(76+31.8/60),78.15,19.822,43+23.4806/60,-(76+58.4779/60),sea},
    {216,2.9,43+27.6/60,-(76+31.8/60),78.15,30.831,43+21.1482/60,-(77+13.2685/60),sea}
  };
  for(const auto& example:examples) {
    cn::VerticalAngleObservation o;
    o.mode=example.mode; o.charted_top_height_m=example.height;
    o.eye_height_m=3; o.index_error_arcmin=-.15;
    o.angle_deg=example.hs_minutes/60;
    const auto result=cn::SolveVerticalAngle(o);
    ASSERT_TRUE(result.valid)<<result.error;
    EXPECT_NEAR(result.range_nm,example.range,.0006);
    const auto fix=cn::Destination({example.target_lat,example.target_lon},
                                   example.bearing_true+180,result.range_nm);
    EXPECT_NEAR(fix.latitude_deg,example.fix_lat,.000002);
    EXPECT_NEAR(fix.longitude_deg,example.fix_lon,.000002);
  }
}

TEST(CoastalNavigation, BobRevisionOneSecondHorizontalFix) {
  cn::HorizontalAngleObservation o;
  o.left={43+55./60,-(69+15.7/60)};
  o.centre={43+57.9/60,-(69+4.4/60)};
  o.right={43+47./60,-(68+51.3/60)};
  o.left_centre_angle_deg=70; o.centre_right_angle_deg=107;
  o.index_error_arcmin=-.15; o.angle_uncertainty_arcmin=60;
  const auto fix=cn::SolveHorizontalThreePointFix(o,{43+49.7/60,-(69+4.6/60)});
  ASSERT_TRUE(fix.valid)<<fix.error;
  EXPECT_NEAR(fix.position.latitude_deg,43+51.3588/60,.000002);
  EXPECT_NEAR(fix.position.longitude_deg,-(69+5.3779/60),.000002);
  EXPECT_NEAR(fix.estimated_uncertainty_nm,.148572,.000001);
}

TEST(CoastalNavigation, BobVerticalExamplesAndSignedAngles) {
  cn::VerticalAngleObservation o;
  o.mode=cn::VerticalAngleMode::SeaHorizonToTopBeyondHorizon;
  o.eye_height_m=3; o.index_error_arcmin=-0.15;
  for (const auto& example : {std::array<double,3>{24,2.9,9.676436},
       {24,2.8,9.797358},{9,2.9,5.169446},{216,14.6,19.821714},
       {216,2.9,30.830644},{24,4.1,8.341}}) {
    o.charted_top_height_m=example[0]; o.angle_deg=example[1]/60;
    const auto r=cn::SolveVerticalAngle(o);
    ASSERT_TRUE(r.valid) << r.error;
    EXPECT_NEAR(r.range_nm,example[2],0.001);
  }
  o.charted_top_height_m=24;
  o.angle_deg=(1.758*std::sqrt(3.)+o.index_error_arcmin)/60;
  const auto zero=cn::SolveVerticalAngle(o);
  ASSERT_TRUE(zero.valid) << zero.error;
  EXPECT_NEAR(zero.corrected_angle_deg,0,1e-15);
  EXPECT_NEAR(zero.range_nm,9.682511,0.001);
}

TEST(CoastalNavigation, BowditchSignedTableEntries) {
  // Table 15, 2019 ed., 70 ft height difference. Published entries are
  // tenths of NM and differ slightly from its printed rounded constants.
  cn::VerticalAngleObservation o;
  o.mode=cn::VerticalAngleMode::SeaHorizonToTopBeyondHorizon;
  o.eye_height_m=10; o.charted_top_height_m=10+70*0.3048;
  for (const auto& example : {std::array<double,2>{-4,15.7},
       {-3,14.0},{-2,12.5},{-1,11.0},{0,9.7},{1,8.6},{3,6.8}}) {
    o.angle_deg=(example[0]+1.758*std::sqrt(10.))/60;
    const auto r=cn::SolveVerticalAngle(o);
    ASSERT_TRUE(r.valid) << r.error;
    EXPECT_NEAR(r.range_nm,example[1],0.1);
  }
}

TEST(CoastalNavigation, VisibilityBoundsAndModeGuidance) {
  cn::VerticalAngleObservation o;
  o.mode=cn::VerticalAngleMode::SeaHorizonToTopBeyondHorizon;
  o.charted_top_height_m=24; o.eye_height_m=3; o.angle_deg=0;
  auto r=cn::SolveVerticalAngle(o);
  ASSERT_TRUE(r.valid) << r.error;
  EXPECT_NEAR(r.range_nm,r.geographic_range_nm,0.01);
  EXPECT_GT(r.geographic_range_nm,13.8);
  EXPECT_LT(r.geographic_range_nm,14.2);
  o.angle_deg=-0.001;
  EXPECT_FALSE(cn::SolveVerticalAngle(o).valid);
  o.angle_deg=1;
  r=cn::SolveVerticalAngle(o);
  ASSERT_TRUE(r.valid);
  EXPECT_GT(r.warnings.size(),1u);
  o.mode=cn::VerticalAngleMode::WaterlineToTop;
  r=cn::SolveVerticalAngle(o);
  o.angle_deg=r.waterline_transition_angle_deg;
  r=cn::SolveVerticalAngle(o);
  ASSERT_TRUE(r.valid) << r.error;
  EXPECT_NEAR(r.range_nm,r.observer_horizon_nm,0.000001);
  o.angle_deg-=0.000001;
  EXPECT_FALSE(cn::SolveVerticalAngle(o).valid);
  for(double angle:{0.,-0.001}) { o.angle_deg=angle; EXPECT_FALSE(cn::SolveVerticalAngle(o).valid); }
  o.angle_deg=1; o.eye_height_m=30;
  EXPECT_FALSE(cn::SolveVerticalAngle(o).valid);
  o.eye_height_m=NAN;
  EXPECT_FALSE(cn::SolveVerticalAngle(o).valid);
}

TEST(CoastalNavigation, BobHorizontalFixAndUncertaintyScaling) {
  cn::HorizontalAngleObservation o;
  o.left={43+45.9/60,-(69+19./60)};
  o.centre={43+57.9/60,-(69+4.4/60)}; o.right={43+47./60,-(68+51.3/60)};
  o.left_centre_angle_deg=111; o.centre_right_angle_deg=107; o.index_error_arcmin=-0.15;
  const cn::GeoPoint initial{43+49.7/60,-(69+4.6/60)};
  auto fine=cn::SolveHorizontalThreePointFix(o,initial);
  ASSERT_TRUE(fine.valid);
  EXPECT_NEAR(fine.position.latitude_deg,43+49.9315/60,0.000002);
  EXPECT_NEAR(fine.position.longitude_deg,-(69+4.4233/60),0.000002);
  o.angle_uncertainty_arcmin=60;
  auto coarse=cn::SolveHorizontalThreePointFix(o,initial);
  ASSERT_TRUE(coarse.valid);
  EXPECT_DOUBLE_EQ(fine.position.latitude_deg,coarse.position.latitude_deg);
  EXPECT_NEAR(coarse.estimated_uncertainty_nm/fine.estimated_uncertainty_nm,300,1e-9);
  for(double sigma:{0.,-1.,double(NAN)}) {
    o.angle_uncertainty_arcmin=sigma;
    EXPECT_FALSE(cn::SolveHorizontalThreePointFix(o,initial).valid);
  }
}

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
  observation.mode = cn::VerticalAngleMode::SeaHorizonToTopBeyondHorizon;
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
  const cn::HorizontalFixResult result =
      cn::SolveHorizontalThreePointFix(observation, cn::GeoPoint{49.96, -1.01});
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_LT(cn::GreatCircleDistanceNm(result.position, truth), 0.001);
  EXPECT_LT(std::fabs(result.first_residual_arcmin), 0.001);
  EXPECT_LT(std::fabs(result.second_residual_arcmin), 0.001);
}

TEST(CoastalNavigation, HorizontalIndexErrorCorrectsBothMeasurements) {
  cn::HorizontalAngleObservation observation;
  observation.left = {50.02, -1.05};
  observation.centre = {50.08, -0.98};
  observation.right = {50.00, -0.90};
  const cn::GeoPoint truth{49.95, -1.00};
  observation.index_error_arcmin = 1.5;
  observation.left_centre_angle_deg =
      cn::IncludedHorizontalAngleDeg(truth, observation.left,
                                     observation.centre) +
      observation.index_error_arcmin / 60.0;
  observation.centre_right_angle_deg =
      cn::IncludedHorizontalAngleDeg(truth, observation.centre,
                                     observation.right) +
      observation.index_error_arcmin / 60.0;
  const cn::HorizontalFixResult result =
      cn::SolveHorizontalThreePointFix(observation, cn::GeoPoint{49.96, -1.01});
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_LT(cn::GreatCircleDistanceNm(result.position, truth), 0.001);
}

TEST(CoastalNavigation, SequentialAnglesRecoverMovingReferencePosition) {
  cn::HorizontalAngleObservation observation;
  observation.left = {50.02, -1.05};
  observation.centre = {50.08, -0.98};
  observation.right = {50.00, -0.90};
  const cn::GeoPoint truth{49.95, -1.00};
  observation.moving_observer = true;
  observation.first_time_offset_seconds = 0.0;
  observation.second_time_offset_seconds = 45.0;
  observation.course_true_deg = 80.0;
  observation.speed_knots = 18.0;
  const cn::GeoPoint second_position =
      cn::Destination(truth, observation.course_true_deg,
                      observation.speed_knots *
                          observation.second_time_offset_seconds / 3600.0);
  observation.left_centre_angle_deg = cn::IncludedHorizontalAngleDeg(
      truth, observation.left, observation.centre);
  observation.centre_right_angle_deg = cn::IncludedHorizontalAngleDeg(
      second_position, observation.centre, observation.right);
  const cn::HorizontalFixResult result =
      cn::SolveHorizontalThreePointFix(observation, cn::GeoPoint{49.96, -1.01});
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_LT(cn::GreatCircleDistanceNm(result.position, truth), 0.001);
  EXPECT_TRUE(std::isfinite(result.estimated_uncertainty_nm));
  EXPECT_GT(result.estimated_uncertainty_nm, 0.0);
}

TEST(CoastalNavigation, SingleHorizontalAngleProducesChartableLocus) {
  const cn::GeoPoint first{53.0, -3.1};
  const cn::GeoPoint second{53.0, -2.9};
  const auto branches = cn::BuildHorizontalAngleLocus(first, second, 40.0, 720);
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
