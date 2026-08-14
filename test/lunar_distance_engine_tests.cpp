#include <gtest/gtest.h>

#include "LunarDistanceEngine.h"

#include <cmath>

namespace ld = lunar_distance;

namespace {

ld::Observation TypicalObservation() {
  ld::Observation observation;
  observation.raw_distance_deg = 59.3;
  observation.moon_altitude_deg = 70.0666666667;
  observation.body_altitude_deg = 17.0166666667;
  observation.moon_altitude_limb = ld::AltitudeLimb::Lower;
  observation.body_altitude_limb = ld::AltitudeLimb::Lower;
  observation.moon_contact = ld::DistanceContact::Near;
  observation.body_contact = ld::DistanceContact::Near;
  observation.index_error_arcmin = -0.8;
  observation.eye_height_m = 2.4;
  observation.pressure_hpa = 1013.0;
  observation.temperature_c = 17.0;
  return observation;
}

ld::EphemerisSample TypicalSample() {
  ld::EphemerisSample sample;
  sample.predicted_distance_deg = 60.0;
  sample.moon_semidiameter_deg = 0.265;
  sample.moon_horizontal_parallax_deg = 0.95;
  sample.body_semidiameter_deg = 0.266;
  sample.body_horizontal_parallax_deg = 0.0024;
  return sample;
}

}  // namespace

TEST(LunarDistanceEngine, ExplicitDistanceContactsHaveCorrectSigns) {
  ld::Observation observation = TypicalObservation();
  const ld::EphemerisSample sample = TypicalSample();
  const ld::Clearance near_near = ld::ClearDistance(observation, sample);
  ASSERT_TRUE(near_near.valid) << near_near.error;

  observation.moon_contact = ld::DistanceContact::Far;
  observation.body_contact = ld::DistanceContact::Far;
  const ld::Clearance far_far = ld::ClearDistance(observation, sample);
  ASSERT_TRUE(far_far.valid) << far_far.error;

  EXPECT_GT(near_near.apparent_distance_deg, far_far.apparent_distance_deg);
  EXPECT_NEAR(near_near.apparent_distance_deg -
                  far_far.apparent_distance_deg,
              2.0 * (sample.moon_semidiameter_deg *
                         (1.0 +
                          std::sin(observation.moon_altitude_deg * M_PI / 180.0) *
                              std::sin(sample.moon_horizontal_parallax_deg *
                                       M_PI / 180.0)) +
                     sample.body_semidiameter_deg),
              0.002);
}

TEST(LunarDistanceEngine, RejectsInconsistentSphericalGeometry) {
  ld::Observation observation = TypicalObservation();
  observation.raw_distance_deg = 170.0;
  observation.moon_altitude_deg = 80.0;
  observation.body_altitude_deg = 80.0;
  const ld::Clearance result = ld::ClearDistance(observation, TypicalSample());
  EXPECT_FALSE(result.valid);
  EXPECT_NE(result.error.find("inconsistent"), std::string::npos);
}

TEST(LunarDistanceEngine, FindsRootToSubSecondPrecision) {
  const ld::Observation observation = TypicalObservation();
  ld::EphemerisSample base = TypicalSample();
  const ld::Clearance clearance = ld::ClearDistance(observation, base);
  ASSERT_TRUE(clearance.valid) << clearance.error;
  const double expected = 3723.4;
  auto ephemeris = [=](double seconds, ld::EphemerisSample* sample,
                       std::string*) {
    *sample = base;
    sample->predicted_distance_deg =
        clearance.cleared_distance_deg + (seconds - expected) / 7200.0;
    return true;
  };
  ld::SolveOptions options;
  options.start_offset_seconds = -10000.0;
  options.end_offset_seconds = 10000.0;
  options.scan_step_seconds = 600.0;
  const ld::SolveResult result =
      ld::SolveTime(observation, ephemeris, options);
  ASSERT_TRUE(result.valid) << result.error;
  ASSERT_EQ(result.candidates.size(), 1u);
  EXPECT_NEAR(result.candidates[0].offset_seconds, expected, 0.1);
  EXPECT_NEAR(result.candidates[0].slope_arcmin_per_hour, 30.0, 0.01);
  EXPECT_GT(result.candidates[0].time_uncertainty_seconds, 0.0);
}

TEST(LunarDistanceEngine, ReportsMultipleTimeCandidates) {
  const ld::Observation observation = TypicalObservation();
  ld::EphemerisSample base = TypicalSample();
  const ld::Clearance clearance = ld::ClearDistance(observation, base);
  ASSERT_TRUE(clearance.valid) << clearance.error;
  auto ephemeris = [=](double seconds, ld::EphemerisSample* sample,
                       std::string*) {
    *sample = base;
    sample->predicted_distance_deg =
        clearance.cleared_distance_deg +
        0.5 * std::sin(seconds * M_PI / 21600.0);
    return true;
  };
  ld::SolveOptions options;
  options.start_offset_seconds = -43000.0;
  options.end_offset_seconds = 43000.0;
  options.scan_step_seconds = 300.0;
  const ld::SolveResult result =
      ld::SolveTime(observation, ephemeris, options);
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_GE(result.candidates.size(), 3u);
  EXPECT_FALSE(result.warnings.empty());
}

TEST(LunarDistanceEngine, ExplainsNoMatch) {
  const ld::Observation observation = TypicalObservation();
  ld::EphemerisSample base = TypicalSample();
  const ld::Clearance clearance = ld::ClearDistance(observation, base);
  ASSERT_TRUE(clearance.valid) << clearance.error;
  auto ephemeris = [=](double, ld::EphemerisSample* sample, std::string*) {
    *sample = base;
    sample->predicted_distance_deg = clearance.cleared_distance_deg + 1.0;
    return true;
  };
  const ld::SolveResult result =
      ld::SolveTime(observation, ephemeris, ld::SolveOptions());
  EXPECT_FALSE(result.valid);
  EXPECT_NE(result.error.find("No matching"), std::string::npos);
}

TEST(LunarDistanceEngine, TwoCorrectedAltitudesRecoverPositionAndLongitude) {
  const ld::GeographicPoint moon_gp{18.0, -42.0};
  const ld::GeographicPoint body_gp{-8.0, 21.0};
  const ld::GeographicPoint truth{36.5, -12.25};
  const double moon_altitude =
      90.0 - ld::GreatCircleDistanceNm(truth, moon_gp) / 60.0;
  const double body_altitude =
      90.0 - ld::GreatCircleDistanceNm(truth, body_gp) / 60.0;
  const ld::PositionResult result = ld::IntersectAltitudeCircles(
      moon_gp, moon_altitude, body_gp, body_altitude);
  ASSERT_TRUE(result.valid) << result.error;
  ASSERT_EQ(result.candidates.size(), 2u);
  double nearest = 1e9;
  for (const auto& candidate : result.candidates)
    nearest = std::min(nearest, ld::GreatCircleDistanceNm(candidate, truth));
  EXPECT_LT(nearest, 1e-5);
  EXPECT_GT(result.circle_crossing_angle_deg, 1.0);
}

TEST(LunarDistanceEngine, InconsistentAltitudesDoNotInventLongitude) {
  const ld::PositionResult result = ld::IntersectAltitudeCircles(
      ld::GeographicPoint{0.0, 0.0}, 89.0,
      ld::GeographicPoint{0.0, 90.0}, 89.0);
  EXPECT_FALSE(result.valid);
  EXPECT_NE(result.error.find("do not intersect"), std::string::npos);
}

TEST(LunarDistanceEngine,
     UnknownConstantWatchOffsetRecoversUtcAndLongitudeEndToEnd) {
  // Simulate a watch which is exactly 4 h 17 min 23.4 s slow but keeps
  // perfect interval time. The celestial GPs and measured centre altitudes
  // are generated from a known position; no longitude is supplied to either
  // solver.
  const double watch_offset_seconds = 4.0 * 3600.0 + 17.0 * 60.0 + 23.4;
  const ld::GeographicPoint truth{28.75, -41.125};
  const ld::GeographicPoint moon_gp{17.25, 36.0};
  const ld::GeographicPoint body_gp{-12.5, -78.0};

  ld::Observation observation;
  observation.moon_altitude_deg =
      90.0 - ld::GreatCircleDistanceNm(truth, moon_gp) / 60.0;
  observation.body_altitude_deg =
      90.0 - ld::GreatCircleDistanceNm(truth, body_gp) / 60.0;
  observation.moon_altitude_limb = ld::AltitudeLimb::Center;
  observation.body_altitude_limb = ld::AltitudeLimb::Center;
  observation.moon_contact = ld::DistanceContact::Center;
  observation.body_contact = ld::DistanceContact::Center;
  observation.eye_height_m = 0.0;
  // A nearly airless synthetic case makes the independently generated
  // geocentric altitudes equal to the entered centre altitudes.
  observation.pressure_hpa = 1e-9;
  observation.temperature_c = 10.0;

  ld::EphemerisSample sample;
  sample.moon_geographic_latitude_deg = moon_gp.latitude_deg;
  sample.moon_geographic_longitude_deg = moon_gp.longitude_deg;
  sample.body_geographic_latitude_deg = body_gp.latitude_deg;
  sample.body_geographic_longitude_deg = body_gp.longitude_deg;
  observation.raw_distance_deg =
      ld::GreatCircleDistanceNm(moon_gp, body_gp) / 60.0;
  const ld::Clearance truth_clearance = ld::ClearDistance(observation, sample);
  ASSERT_TRUE(truth_clearance.valid) << truth_clearance.error;

  auto ephemeris = [=](double candidate_offset, ld::EphemerisSample* out,
                       std::string*) {
    *out = sample;
    // Typical lunar distances change by roughly half a minute of arc per
    // minute of time. Only the constant watch offset is unknown.
    out->predicted_distance_deg =
        truth_clearance.cleared_distance_deg +
        (candidate_offset - watch_offset_seconds) * 0.5 / 3600.0;
    return true;
  };
  ld::SolveOptions options;
  options.start_offset_seconds = -86400.0;
  options.end_offset_seconds = 86400.0;
  options.scan_step_seconds = 300.0;
  const ld::SolveResult time =
      ld::SolveTime(observation, ephemeris, options);
  ASSERT_TRUE(time.valid) << time.error;
  ASSERT_EQ(time.candidates.size(), 1u);
  EXPECT_NEAR(time.candidates[0].offset_seconds, watch_offset_seconds, 0.1);

  ld::EphemerisSample recovered_sample;
  std::string error;
  ASSERT_TRUE(ephemeris(time.candidates[0].offset_seconds, &recovered_sample,
                        &error));
  const ld::Clearance recovered =
      ld::ClearDistance(observation, recovered_sample);
  ASSERT_TRUE(recovered.valid) << recovered.error;
  const ld::PositionResult position = ld::IntersectAltitudeCircles(
      moon_gp, recovered.moon_geocentric_altitude_deg, body_gp,
      recovered.body_geocentric_altitude_deg);
  ASSERT_TRUE(position.valid) << position.error;
  double nearest = 1e9;
  for (const auto& candidate : position.candidates)
    nearest = std::min(nearest, ld::GreatCircleDistanceNm(candidate, truth));
  EXPECT_LT(nearest, 1e-4);
}

TEST(LunarDistanceEngine,
     SeparateWatchTimesRecoverClockAndMovingReferencePosition) {
  const double truth_correction =
      4.0 * 3600.0 + 17.0 * 60.0 + 23.4;
  const ld::GeographicPoint truth{28.75, -41.125};

  ld::Observation observation;
  observation.separate_times = true;
  observation.moon_time_offset_seconds = -12.0;
  observation.body_time_offset_seconds = 9.0;
  observation.moving_observer = true;
  observation.course_true_deg = 72.0;
  observation.speed_knots = 7.0;
  observation.moon_altitude_limb = ld::AltitudeLimb::Lower;
  observation.body_altitude_limb = ld::AltitudeLimb::Lower;
  observation.moon_contact = ld::DistanceContact::Near;
  observation.body_contact = ld::DistanceContact::Near;
  observation.index_error_arcmin = -0.7;
  observation.eye_height_m = 2.5;
  observation.pressure_hpa = 1008.0;
  observation.temperature_c = 16.0;
  observation.distance_uncertainty_arcmin = 0.2;
  observation.moon_altitude_uncertainty_arcmin = 0.3;
  observation.body_altitude_uncertainty_arcmin = 0.3;

  auto ephemeris = [=](double candidate_correction,
                       ld::EphemerisSample* sample, std::string*) {
    const double hours =
        (candidate_correction - truth_correction) / 3600.0;
    sample->moon_geographic_latitude_deg = 18.0 + 0.05 * hours;
    sample->moon_geographic_longitude_deg = 36.0 - 14.45 * hours;
    sample->body_geographic_latitude_deg = -12.5 + 0.01 * hours;
    sample->body_geographic_longitude_deg = -78.0 - 15.0 * hours;
    sample->moon_semidiameter_deg = 0.265;
    sample->moon_horizontal_parallax_deg = 0.95;
    sample->body_semidiameter_deg = 0.266;
    sample->body_horizontal_parallax_deg = 0.0024;
    sample->predicted_distance_deg =
        ld::GreatCircleDistanceNm(
            {sample->moon_geographic_latitude_deg,
             sample->moon_geographic_longitude_deg},
            {sample->body_geographic_latitude_deg,
             sample->body_geographic_longitude_deg}) /
        60.0;
    return true;
  };

  const ld::PredictedObservation generated =
      ld::PredictTimeTaggedObservation(observation, ephemeris,
                                       truth_correction, truth);
  ASSERT_TRUE(generated.valid) << generated.error;
  observation.raw_distance_deg = generated.raw_distance_deg;
  observation.moon_altitude_deg = generated.moon_altitude_deg;
  observation.body_altitude_deg = generated.body_altitude_deg;

  ld::SolveOptions options;
  options.start_offset_seconds = -21600.0;
  options.end_offset_seconds = 21600.0;
  options.scan_step_seconds = 300.0;
  const ld::SolveResult solved =
      ld::SolveTimeTagged(observation, ephemeris, options);
  ASSERT_TRUE(solved.valid) << solved.error;
  double best_time_error = 1e9;
  double best_position_error = 1e9;
  double best_time_uncertainty = INFINITY;
  double best_position_uncertainty = INFINITY;
  for (const ld::TimeCandidate& candidate : solved.candidates) {
    const double time_error =
        std::fabs(candidate.offset_seconds - truth_correction);
    if (time_error < best_time_error) {
      best_time_error = time_error;
      best_position_error = 1e9;
      best_time_uncertainty = candidate.time_uncertainty_seconds;
      best_position_uncertainty = candidate.position_uncertainty_nm;
      for (const ld::GeographicPoint& position : candidate.positions)
        best_position_error = std::min(
            best_position_error,
            ld::GreatCircleDistanceNm(position, truth));
    }
  }
  EXPECT_LT(best_time_error, 0.1)
      << "nearest recovered correction error in seconds";
  EXPECT_LT(best_position_error, 0.005)
      << "position error at nearest recovered correction in NM";
  EXPECT_TRUE(std::isfinite(best_time_uncertainty));
  EXPECT_GT(best_time_uncertainty, 0.0);
  EXPECT_TRUE(std::isfinite(best_position_uncertainty));
  EXPECT_GT(best_position_uncertainty, 0.0);

  // Treating these genuinely sequential angles as simultaneous must not be
  // allowed to masquerade as the same solution.
  ld::Observation simultaneous = observation;
  simultaneous.separate_times = false;
  simultaneous.moving_observer = false;
  const ld::SolveResult mistagged =
      ld::SolveTime(simultaneous, ephemeris, options);
  double mistagged_time_error = INFINITY;
  for (const ld::TimeCandidate& candidate : mistagged.candidates)
    mistagged_time_error =
        std::min(mistagged_time_error,
                 std::fabs(candidate.offset_seconds - truth_correction));
  EXPECT_GT(mistagged_time_error, 1.0)
      << "discarding the individual watch times should measurably bias UTC";
}
