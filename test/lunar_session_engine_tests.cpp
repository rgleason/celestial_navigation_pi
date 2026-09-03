#include <gtest/gtest.h>

#include "LunarSessionEngine.h"

#include <atomic>
#include <cmath>

namespace {

lunar_distance::EphemerisFunction Ephemeris(int body) {
  return [body](double seconds, lunar_distance::EphemerisSample* sample,
                std::string*) {
    const double hours = seconds / 3600.0;
    sample->predicted_distance_deg = 35.0 + body * 7.0 +
                                     (0.45 + body * 0.035) * hours +
                                     0.04 * std::sin(hours * 0.7 + body);
    sample->moon_semidiameter_deg = 0.25;
    sample->moon_horizontal_parallax_deg = 0.95;
    sample->body_semidiameter_deg = 0.0;
    sample->body_horizontal_parallax_deg = 0.0;
    sample->moon_geographic_latitude_deg = 12.0 + 0.15 * hours;
    sample->moon_geographic_longitude_deg = 25.0 + 14.45 * hours;
    sample->body_geographic_latitude_deg = -20.0 + body * 16.0;
    sample->body_geographic_longitude_deg = -80.0 + body * 57.0 + 15.02 * hours;
    return true;
  };
}

constexpr double kPi = 3.14159265358979323846;

lunar_distance::GeographicPoint Advance(
    const lunar_distance::GeographicPoint& start, double course_deg,
    double distance_nm) {
  const double angular = distance_nm / 60.0 * kPi / 180.0;
  const double bearing = course_deg * kPi / 180.0;
  const double latitude = start.latitude_deg * kPi / 180.0;
  const double longitude = start.longitude_deg * kPi / 180.0;
  const double end_latitude =
      std::asin(std::sin(latitude) * std::cos(angular) +
                std::cos(latitude) * std::sin(angular) * std::cos(bearing));
  const double end_longitude =
      longitude +
      std::atan2(
          std::sin(bearing) * std::sin(angular) * std::cos(latitude),
          std::cos(angular) - std::sin(latitude) * std::sin(end_latitude));
  return {end_latitude * 180.0 / kPi, end_longitude * 180.0 / kPi};
}

std::vector<lunar_session::SessionObservation> MakeSession(
    double correction, const lunar_distance::GeographicPoint& position,
    int count = 5, double common_bias_arcmin = 0.0, bool moving = false,
    double course_deg = 0.0, double speed_knots = 0.0) {
  std::vector<lunar_session::SessionObservation> result;
  for (int index = 0; index < count; ++index) {
    lunar_session::SessionObservation entry;
    entry.label = "Lunar " + std::to_string(index + 1);
    entry.ephemeris = Ephemeris(index % 3);
    entry.epoch_offset_seconds = index * 780.0;
    entry.settings.moon_altitude_limb = lunar_distance::AltitudeLimb::Center;
    entry.settings.body_altitude_limb = lunar_distance::AltitudeLimb::Center;
    entry.settings.moon_contact = lunar_distance::DistanceContact::Center;
    entry.settings.body_contact = lunar_distance::DistanceContact::Center;
    entry.settings.distance_uncertainty_arcmin = 0.25;
    entry.settings.moon_altitude_uncertainty_arcmin = 0.35;
    entry.settings.body_altitude_uncertainty_arcmin = 0.35;
    auto shifted = [&entry](double seconds,
                            lunar_distance::EphemerisSample* sample,
                            std::string* error) {
      return entry.ephemeris(seconds + entry.epoch_offset_seconds, sample,
                             error);
    };
    lunar_distance::Observation generating_settings = entry.settings;
    generating_settings.index_error_arcmin += common_bias_arcmin;
    const auto observation_position =
        moving ? Advance(position, course_deg,
                         speed_knots * entry.epoch_offset_seconds / 3600.0)
               : position;
    const auto predicted = lunar_distance::PredictTimeTaggedObservation(
        generating_settings, shifted, correction, observation_position);
    EXPECT_TRUE(predicted.valid) << predicted.error;
    entry.settings.raw_distance_deg = predicted.raw_distance_deg;
    entry.settings.moon_altitude_deg = predicted.moon_altitude_deg;
    entry.settings.body_altitude_deg = predicted.body_altitude_deg;
    result.push_back(entry);
  }
  return result;
}

TEST(LunarSessionEngine, RecoversClockAndPositionJointly) {
  const lunar_distance::GeographicPoint truth(32.4, -48.7);
  const double correction = 4372.0;
  auto observations = MakeSession(correction, truth);
  lunar_session::Options options;
  options.start_correction_seconds = -6.0 * 3600.0;
  options.end_correction_seconds = 6.0 * 3600.0;
  options.correction_seed_step_seconds = 1800.0;
  options.known_or_initial_position = {30.0, -45.0};
  options.position_seeds = {{30.0, -45.0}, {35.0, -50.0}};
  const auto result = lunar_session::Solve(observations, options);
  ASSERT_TRUE(result.valid) << result.error;
  ASSERT_FALSE(result.candidates.empty());
  EXPECT_NEAR(result.candidates[0].clock_correction_seconds, correction, 1.0);
  EXPECT_LT(lunar_distance::GreatCircleDistanceNm(
                result.candidates[0].reference_position, truth),
            0.1);
  EXPECT_LT(result.candidates[0].angular_rms_arcmin, 0.01);
}

TEST(LunarSessionEngine, RobustFitReportsButResistsAnOutlier) {
  const lunar_distance::GeographicPoint truth(18.0, 122.0);
  const double correction = -2710.0;
  auto observations = MakeSession(correction, truth, 7);
  observations[3].settings.raw_distance_deg += 4.0 / 60.0;
  lunar_session::Options options;
  options.start_correction_seconds = -4.0 * 3600.0;
  options.end_correction_seconds = 4.0 * 3600.0;
  options.correction_seed_step_seconds = 1800.0;
  options.known_or_initial_position = {20.0, 120.0};
  options.position_seeds = {{20.0, 120.0}};
  options.robust_fit = true;
  const auto result = lunar_session::Solve(observations, options);
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_NEAR(result.candidates[0].clock_correction_seconds, correction, 20.0);
  EXPECT_LT(lunar_distance::GreatCircleDistanceNm(
                result.candidates[0].reference_position, truth),
            5.0);
  EXPECT_TRUE(result.candidates[0].residuals[3].possible_outlier);
}

TEST(LunarSessionEngine, KnownPositionRecoversTimeWithoutPositionFit) {
  const lunar_distance::GeographicPoint truth(-21.5, 14.2);
  const double correction = 9325.0;
  auto observations = MakeSession(correction, truth, 3);
  lunar_session::Options options;
  options.solve_position = false;
  options.known_or_initial_position = truth;
  options.start_correction_seconds = 0.0;
  options.end_correction_seconds = 4.0 * 3600.0;
  options.correction_seed_step_seconds = 1200.0;
  const auto result = lunar_session::Solve(observations, options);
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_NEAR(result.candidates[0].clock_correction_seconds, correction, 1.0);
  EXPECT_DOUBLE_EQ(result.candidates[0].reference_position.latitude_deg,
                   truth.latitude_deg);
}

TEST(LunarSessionEngine, RecoversCommonBiasWhenSequenceIsRedundant) {
  const lunar_distance::GeographicPoint truth(41.2, -16.8);
  const double correction = 1850.0;
  const double bias = 0.85;
  auto observations = MakeSession(correction, truth, 7, bias);
  lunar_session::Options options;
  options.start_correction_seconds = -2.0 * 3600.0;
  options.end_correction_seconds = 2.0 * 3600.0;
  options.correction_seed_step_seconds = 1200.0;
  options.known_or_initial_position = {40.0, -15.0};
  options.position_seeds = {{40.0, -15.0}};
  options.estimate_common_index_bias = true;
  const auto result = lunar_session::Solve(observations, options);
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_NEAR(result.candidates[0].clock_correction_seconds, correction, 2.0);
  EXPECT_NEAR(result.candidates[0].common_index_bias_arcmin, bias, 0.02);
  EXPECT_LT(lunar_distance::GreatCircleDistanceNm(
                result.candidates[0].reference_position, truth),
            0.2);
}

TEST(LunarSessionEngine, AdvancesASequenceToItsReferenceEpoch) {
  const lunar_distance::GeographicPoint truth(-12.0, 88.0);
  const double correction = -1450.0;
  const double course = 73.0;
  const double speed = 18.0;
  auto observations =
      MakeSession(correction, truth, 8, 0.0, true, course, speed);
  lunar_session::Options options;
  options.start_correction_seconds = -2.0 * 3600.0;
  options.end_correction_seconds = 2.0 * 3600.0;
  options.correction_seed_step_seconds = 1200.0;
  options.known_or_initial_position = {-11.0, 87.0};
  options.position_seeds = {{-11.0, 87.0}};
  options.moving_observer = true;
  options.course_true_deg = course;
  options.speed_knots = speed;
  const auto result = lunar_session::Solve(observations, options);
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_NEAR(result.candidates[0].clock_correction_seconds, correction, 2.0);
  EXPECT_LT(lunar_distance::GreatCircleDistanceNm(
                result.candidates[0].reference_position, truth),
            0.2);
}

TEST(LunarSessionEngine, RejectsExactlyDeterminedBiasFit) {
  const lunar_distance::GeographicPoint truth(10.0, 10.0);
  auto observations = MakeSession(0.0, truth, 1);
  lunar_session::Options options;
  options.estimate_common_index_bias = true;
  const auto result = lunar_session::Solve(observations, options);
  EXPECT_FALSE(result.valid);
  EXPECT_NE(result.error.find("not overdetermined"), std::string::npos);
}

TEST(LunarSessionEngine, BoundsHistoricMultiStartSeeds) {
  const lunar_distance::GeographicPoint truth(32.4, -48.7);
  auto observations = MakeSession(900.0, truth);
  lunar_session::Options options;
  options.start_correction_seconds = -6.0 * 3600.0;
  options.end_correction_seconds = 6.0 * 3600.0;
  options.correction_seed_step_seconds = 300.0;
  for (int index = -100; index <= 100; ++index)
    options.correction_seeds.push_back(index * 180.0);
  for (int index = 0; index < 20; ++index)
    options.position_seeds.push_back({30.0 + index, -45.0 + index});
  options.maximum_correction_seeds = 7;
  options.maximum_position_seeds = 3;
  std::size_t reported_total = 0;
  std::size_t reported_complete = 0;
  options.progress = [&](std::size_t complete, std::size_t total) {
    reported_complete = complete;
    reported_total = total;
  };
  const auto result = lunar_session::Solve(observations, options);
  EXPECT_LE(reported_total, 21u);
  EXPECT_EQ(reported_complete, reported_total);
  EXPECT_TRUE(result.valid) << result.error;
}

TEST(LunarSessionEngine, CancellationStopsMultiStartFit) {
  const lunar_distance::GeographicPoint truth(32.4, -48.7);
  auto observations = MakeSession(900.0, truth, 8);
  lunar_session::Options options;
  options.start_correction_seconds = -12.0 * 3600.0;
  options.end_correction_seconds = 12.0 * 3600.0;
  options.correction_seed_step_seconds = 300.0;
  options.position_seeds = {{30.0, -45.0}, {35.0, -50.0}};
  std::atomic<bool> cancel(false);
  options.cancel_requested = [&]() { return cancel.load(); };
  options.progress = [&](std::size_t complete, std::size_t) {
    if (complete >= 1) cancel.store(true);
  };
  const auto result = lunar_session::Solve(observations, options);
  EXPECT_FALSE(result.valid);
  EXPECT_NE(result.error.find("cancelled"), std::string::npos);
}

TEST(LunarSessionEngine, RejectsAnUnboundedHistoricArchiveAsOneSession) {
  const lunar_distance::GeographicPoint truth(32.4, -48.7);
  auto observations = MakeSession(900.0, truth, 13);
  lunar_session::Options options;
  options.known_or_initial_position = truth;
  const auto result = lunar_session::Solve(observations, options);
  EXPECT_FALSE(result.valid);
  EXPECT_NE(result.error.find("Too many observations"), std::string::npos);
}

TEST(LunarSessionEngine, RejectsObservationsSpanningMultipleDays) {
  const lunar_distance::GeographicPoint truth(32.4, -48.7);
  auto observations = MakeSession(900.0, truth, 3);
  observations.back().epoch_offset_seconds = 2.0 * 86400.0;
  lunar_session::Options options;
  options.known_or_initial_position = truth;
  const auto result = lunar_session::Solve(observations, options);
  EXPECT_FALSE(result.valid);
  EXPECT_NE(result.error.find("span more"), std::string::npos);
}

}  // namespace
