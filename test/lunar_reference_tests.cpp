#include "lunar_reference_support.hpp"

TEST(LunarReference, All32CasesRecoverAcrossStepsAndObserverCallbacks) {
  auto cases = Cases();
  ASSERT_EQ(cases.size(), 32u);
  for (const auto& c : cases)
    for (bool callback : {false, true})
      for (bool tagged : {false, true})
        for (double step : {7., 30., 60., 300.}) {
          SCOPED_TRACE(c.name + " step=" + std::to_string(step) +
                       " callback=" + std::to_string(callback) +
                       " tagged=" + std::to_string(tagged));
          auto p = Reference(c, callback);
          auto o = Generate(Settings(c, tagged), p, 0, c.observer);
          auto result = ld::SolveTime(o, p, Options(step));
          ASSERT_TRUE(result.valid) << result.error;
          EXPECT_TRUE(Recovers(result, 0, c.observer));
        }
}
TEST(LunarReference, OriginalArcturusBoundaryAndNeighboursRecover) {
  auto cases = Cases();
  auto it = std::find_if(cases.begin(), cases.end(),
                         [](const auto& c) { return c.name == "29-Arcturus"; });
  ASSERT_NE(it, cases.end());
  auto p = Reference(*it);
  for (double lat : {84.8, 84.9, 85., 85.1, 85.2})
    for (double dlon : {-.1, 0., .1})
      for (double t : {-5., 0., 5.})
        for (double step : {30., 300.}) {
          const ld::GeographicPoint truth{lat,
                                          it->observer.longitude_deg + dlon};
          SCOPED_TRACE(
              "lat=" + std::to_string(lat) + " dlon=" + std::to_string(dlon) +
              " time=" + std::to_string(t) + " step=" + std::to_string(step));
          auto o = Generate(Settings(*it), p, t, truth);
          auto r = ld::SolveTime(o, p, Options(step));
          ASSERT_TRUE(r.valid) << r.error;
          EXPECT_TRUE(Recovers(r, t, truth));
        }
}
TEST(LunarReference, ReturnedNoisySolutionsReproduceAllThreeObservedAngles) {
  auto cases = Cases();
  std::mt19937 rng(20261002);
  std::normal_distribution<double> noise(0, .02 / 60);
  int valid = 0, failed = 0;
  auto evidence = NoiseEvidence("noise-0.02-arcmin.jsonl");
  for (const auto& c : cases)
    for (int repeat = 0; repeat < 8; ++repeat) {
      auto p = Reference(c, true);
      auto o = Generate(Settings(c), p, 0, c.observer);
      o.raw_distance_deg += noise(rng);
      o.moon_altitude_deg += noise(rng);
      o.body_altitude_deg += noise(rng);
      auto r = ld::SolveTime(o, p, Options(30));
      RecordNoise(evidence, c, repeat, o, r);
      if (!r.valid) {
        ++failed;
        EXPECT_FALSE(r.error.empty());
        continue;
      }
      ++valid;
      for (const auto& candidate : r.candidates)
        for (const auto& position : candidate.positions) {
          auto f = ld::PredictTimeTaggedObservation(
              o, p, candidate.offset_seconds, position);
          ASSERT_TRUE(f.valid);
          EXPECT_NEAR(f.raw_distance_deg, o.raw_distance_deg, 1e-6);
          EXPECT_NEAR(f.moon_altitude_deg, o.moon_altitude_deg, 1e-8);
          EXPECT_NEAR(f.body_altitude_deg, o.body_altitude_deg, 1e-8);
        }
    }
  EXPECT_GE(valid, 200);
  EXPECT_EQ(valid + failed, 256);
  RecordProperty("noisy_solutions", valid);
  RecordProperty("noisy_rejections", failed);
}

TEST(LunarReference, LosingFirstIntersectionDoesNotRelabelSecondBranch) {
  bool exercised = false;
  for (const auto& c : Cases()) {
    auto reference = Reference(c, true);
    constexpr double root = .25;
    auto shifted = [=](double t, ld::EphemerisSample* s, std::string* e) {
      return reference(t - root, s, e);
    };
    auto o = Generate(Settings(c), shifted, root, c.observer);
    auto positions = ld::PositionAtTime(o, shifted, root);
    if (positions.candidates.size() != 2 ||
        ld::GreatCircleDistanceNm(positions.candidates[1], c.observer) > .001)
      continue;
    const auto first = positions.candidates[0];
    const auto second = positions.candidates[1];
    auto partial = [=](double t, ld::EphemerisSample* s, std::string* e) {
      if (!shifted(t, s, e)) return false;
      auto original = s->observer_direction;
      s->observer_direction = [=](double lat, double lon, double height,
                                  bool moon, double* alt, double* az,
                                  double* sd) {
        if (t < .1 && ld::GreatCircleDistanceNm({lat, lon}, first) <
                          ld::GreatCircleDistanceNm({lat, lon}, second))
          return false;
        return original(lat, lon, height, moon, alt, az, sd);
      };
      return true;
    };
    auto only_second = ld::PositionAtTime(o, partial, 0);
    ASSERT_TRUE(only_second.valid);
    ASSERT_EQ(only_second.candidates.size(), 1u);
    auto result = ld::SolveTime(o, partial, Options(30));
    ASSERT_TRUE(result.valid) << result.error;
    EXPECT_TRUE(Recovers(result, root, c.observer));
    for (const auto& candidate : result.candidates)
      for (const auto& position : candidate.positions) {
        auto f = ld::PredictTimeTaggedObservation(
            o, partial, candidate.offset_seconds, position);
        ASSERT_TRUE(f.valid);
        EXPECT_NEAR(f.raw_distance_deg, o.raw_distance_deg, 1e-6);
        EXPECT_NEAR(f.moon_altitude_deg, o.moon_altitude_deg, 1e-8);
        EXPECT_NEAR(f.body_altitude_deg, o.body_altitude_deg, 1e-8);
      }
    exercised = true;
    break;
  }
  EXPECT_TRUE(exercised);
}

TEST(LunarReference,
     RealisticNoiseReturnsConsistentSolutionsOrExplicitRejections) {
  std::mt19937 rng(20261003);
  std::normal_distribution<double> noise(0, .2 / 60);
  int valid = 0, failed = 0;
  auto evidence = NoiseEvidence("noise-0.2-arcmin.jsonl");
  auto fine_evidence = NoiseEvidence("noise-0.2-rejections-fine.jsonl");
  auto extended_evidence = NoiseEvidence("noise-0.2-rejections-extended.jsonl");
  for (const auto& c : Cases())
    for (int repeat = 0; repeat < 8; ++repeat) {
      auto p = Reference(c, true);
      auto o = Generate(Settings(c), p, 0, c.observer);
      o.raw_distance_deg += noise(rng);
      o.moon_altitude_deg += noise(rng);
      o.body_altitude_deg += noise(rng);
      auto options = Options(30);
      options.start_offset_seconds = -500;
      options.end_offset_seconds = 500;
      auto r = ld::SolveTime(o, p, options);
      RecordNoise(evidence, c, repeat, o, r);
      if (!r.valid) {
        ++failed;
        EXPECT_FALSE(r.error.empty());
        EXPECT_TRUE(r.candidates.empty());
        // Verify that these rejections are not caused by the 30-second scan.
        options.scan_step_seconds = 1;
        auto fine = ld::SolveTime(o, p, options);
        RecordNoise(fine_evidence, c, repeat, o, fine);
        EXPECT_FALSE(fine.valid) << c.name << " trial " << repeat;
        options.start_offset_seconds = -599;
        options.end_offset_seconds = 599;
        auto extended = ld::SolveTime(o, p, options);
        RecordNoise(extended_evidence, c, repeat, o, extended);
        continue;
      }
      ++valid;
      for (const auto& candidate : r.candidates)
        for (const auto& position : candidate.positions) {
          auto f = ld::PredictTimeTaggedObservation(
              o, p, candidate.offset_seconds, position);
          ASSERT_TRUE(f.valid);
          EXPECT_NEAR(f.raw_distance_deg, o.raw_distance_deg, 1e-6);
          EXPECT_NEAR(f.moon_altitude_deg, o.moon_altitude_deg, 1e-8);
          EXPECT_NEAR(f.body_altitude_deg, o.body_altitude_deg, 1e-8);
        }
    }
  EXPECT_GE(valid, 200);
  EXPECT_EQ(valid + failed, 256);
  RecordProperty("realistic_noise_solutions", valid);
  RecordProperty("realistic_noise_rejections", failed);
}

TEST(LunarReference, Wgs84SolutionsBeyondSphericalSeedBoundaryAreRecovered) {
  std::ifstream input(std::string(LUNAR_REFERENCE_DIR) +
                      "/noisy-boundary-regressions.tsv");
  ASSERT_TRUE(input.is_open());
  std::string line;
  std::getline(input, line);
  int count = 0;
  const auto cases = Cases();
  while (std::getline(input, line)) {
    auto f = Fields(line);
    const auto c = std::find_if(cases.begin(), cases.end(),
                                [&](const auto& c) { return c.name == f[0]; });
    ASSERT_NE(c, cases.end());
    auto p = Reference(*c, true);
    auto o = Settings(*c);
    o.raw_distance_deg = std::stod(f[2]);
    o.moon_altitude_deg = std::stod(f[3]);
    o.body_altitude_deg = std::stod(f[4]);
    const double t = std::stod(f[5]);
    const ld::GeographicPoint truth{std::stod(f[6]), std::stod(f[7])};
    auto forward = ld::PredictTimeTaggedObservation(o, p, t, truth);
    ASSERT_TRUE(forward.valid);
    EXPECT_NEAR(forward.raw_distance_deg, o.raw_distance_deg, 1e-8);
    EXPECT_NEAR(forward.moon_altitude_deg, o.moon_altitude_deg, 1e-8);
    EXPECT_NEAR(forward.body_altitude_deg, o.body_altitude_deg, 1e-8);
    for (double step : {7., 30., 300.}) {
      auto r = ld::SolveTime(o, p, Options(step));
      ASSERT_TRUE(r.valid) << r.error << " trial " << f[1];
      EXPECT_TRUE(Recovers(r, t, truth)) << " trial " << f[1];
    }
    ++count;
  }
  EXPECT_EQ(count, 2);
}
