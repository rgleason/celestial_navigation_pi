#include "LunarDistanceEngine.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <future>
#include <limits>
#include <map>
namespace ld = lunar_distance;
namespace {
const ld::GeographicPoint truth{28.75, -41.125};
ld::Observation Settings(bool ellipsoid = true) {
  ld::Observation o;
  o.use_ellipsoid = ellipsoid;
  o.separate_times = !ellipsoid;
  o.moon_altitude_limb = ld::AltitudeLimb::Lower;
  o.body_altitude_limb = ld::AltitudeLimb::Upper;
  o.moon_contact = ld::DistanceContact::Near;
  o.body_contact = ld::DistanceContact::Far;
  o.index_error_arcmin = -.7;
  o.eye_height_m = 2.5;
  o.pressure_hpa = 1008;
  o.temperature_c = 16;
  return o;
}
ld::EphemerisFunction Provider(double root = 0) {
  return [root](double t, ld::EphemerisSample* s, std::string*) {
    const double h = (t - root) / 3600;
    *s = {};
    s->moon_geographic_latitude_deg = 18 + .05 * h;
    s->moon_geographic_longitude_deg = 36 - 14.45 * h;
    s->body_geographic_latitude_deg = -12.5 + .01 * h;
    s->body_geographic_longitude_deg = -78 - 15 * h;
    s->moon_semidiameter_deg = .265;
    s->moon_horizontal_parallax_deg = .95;
    s->body_semidiameter_deg = .266;
    s->body_horizontal_parallax_deg = .0024;
    s->predicted_distance_deg =
        ld::GreatCircleDistanceNm(
            {s->moon_geographic_latitude_deg, s->moon_geographic_longitude_deg},
            {s->body_geographic_latitude_deg,
             s->body_geographic_longitude_deg}) /
        60;
    return true;
  };
}
ld::Observation Observation(const ld::Observation& settings,
                            const ld::EphemerisFunction& provider, double t) {
  auto p = ld::PredictTimeTaggedObservation(settings, provider, t, truth);
  EXPECT_TRUE(p.valid) << p.error;
  auto o = settings;
  o.raw_distance_deg = p.raw_distance_deg;
  o.moon_altitude_deg = p.moon_altitude_deg;
  o.body_altitude_deg = p.body_altitude_deg;
  return o;
}
ld::SolveOptions Options(double start = -60, double end = 60,
                         double step = 30) {
  ld::SolveOptions o;
  o.start_offset_seconds = start;
  o.end_offset_seconds = end;
  o.scan_step_seconds = step;
  o.root_tolerance_seconds = .0005;
  return o;
}
const ld::TimeCandidate* Nearest(const ld::SolveResult& r, double t) {
  if (r.candidates.empty()) return nullptr;
  return &*std::min_element(r.candidates.begin(), r.candidates.end(),
                            [t](const auto& a, const auto& b) {
                              return std::fabs(a.offset_seconds - t) <
                                     std::fabs(b.offset_seconds - t);
                            });
}
bool Warning(const ld::SolveResult& r, const std::string& part) {
  return std::any_of(r.warnings.begin(), r.warnings.end(), [&](const auto& s) {
    return s.find(part) != std::string::npos;
  });
}
}  // namespace
TEST(LunarBoundary, RefinesInvalidToValidBoundaryWithoutChangingCoarseStep) {
  for (bool reverse : {false, true}) {
    const double root = reverse ? .156 : -.156;
    auto p = Provider(root);
    auto o = Observation(Settings(), p, root);
    auto limited = [=](double t, ld::EphemerisSample* s, std::string* e) {
      if (reverse ? t > .3 : t < -.3) {
        if (e) *e = "provider boundary";
        return false;
      }
      return p(t, s, e);
    };
    auto r = ld::SolveTime(o, limited, Options());
    ASSERT_TRUE(r.valid) << r.error;
    auto c = Nearest(r, root);
    ASSERT_NE(c, nullptr);
    EXPECT_NEAR(c->offset_seconds, root, .0005);
    EXPECT_TRUE(c->local_slope_available);
    EXPECT_TRUE(c->uncertainty_available);
  }
}
TEST(LunarBoundary, EndpointsUseOneSidedDiagnosticsInsideSearchBounds) {
  for (double root : {-60., 60.}) {
    auto p = Provider(root);
    auto o = Observation(Settings(), p, root);
    int outside = 0;
    auto limited = [&](double t, ld::EphemerisSample* s, std::string* e) {
      if (t < -60 || t > 60) {
        ++outside;
        return false;
      }
      return p(t, s, e);
    };
    auto r = ld::SolveTime(o, limited, Options());
    ASSERT_TRUE(r.valid) << r.error;
    auto c = Nearest(r, root);
    ASSERT_NE(c, nullptr);
    EXPECT_DOUBLE_EQ(c->offset_seconds, root);
    EXPECT_TRUE(c->local_slope_available);
    EXPECT_TRUE(c->used_one_sided_slope);
    EXPECT_TRUE(c->uncertainty_available);
    EXPECT_EQ(outside, 0);
    EXPECT_TRUE(Warning(r, "one-sided"));
  }
}
TEST(LunarBoundary, ValidRootSurvivesUnavailableDiagnostics) {
  auto p = Provider();
  auto o = Observation(Settings(), p, 0);
  auto only_root = [=](double t, ld::EphemerisSample* s, std::string* e) {
    return t == 0 && p(t, s, e);
  };
  auto r = ld::SolveTime(o, only_root, Options());
  ASSERT_TRUE(r.valid) << r.error;
  auto c = Nearest(r, 0);
  ASSERT_NE(c, nullptr);
  EXPECT_DOUBLE_EQ(c->offset_seconds, 0);
  EXPECT_FALSE(c->local_slope_available);
  EXPECT_FALSE(c->uncertainty_available);
  EXPECT_TRUE(std::isnan(c->slope_arcmin_per_hour));
  EXPECT_TRUE(std::isinf(c->time_uncertainty_seconds));
  EXPECT_TRUE(Warning(r, "unavailable"));
}
TEST(LunarBoundary, InvalidGapDoesNotManufactureAZeroCrossing) {
  auto p = Provider();
  auto settings = Settings(false);
  // With spherical simultaneous centres both intersections have the same raw
  // distance, so neither physical branch can supply a different valid root.
  settings.moon_contact = ld::DistanceContact::Center;
  settings.body_contact = ld::DistanceContact::Center;
  auto o = Observation(settings, p, 0);
  auto gap = [=](double t, ld::EphemerisSample* s, std::string* e) {
    return std::fabs(t) > .1 && p(t, s, e);
  };
  auto r = ld::SolveTimeTagged(o, gap, Options(-30, 30, 60));
  EXPECT_FALSE(r.valid);
  EXPECT_TRUE(r.candidates.empty());
}

TEST(LunarBoundary, ValidButDiscontinuousProviderDoesNotManufactureARoot) {
  auto p = Provider();
  auto settings = Settings(false);
  settings.moon_contact = ld::DistanceContact::Center;
  settings.body_contact = ld::DistanceContact::Center;
  auto o = Observation(settings, p, 0);
  auto jump = [=](double t, ld::EphemerisSample* s, std::string* e) {
    return p(t < 0 ? -1 : 1, s, e);
  };
  auto r = ld::SolveTimeTagged(o, jump, Options(-30, 30, 60));
  EXPECT_FALSE(r.valid);
  EXPECT_TRUE(r.candidates.empty());
}
TEST(LunarBoundary, FindsTwoRootsOnOppositeSidesOfAnInvalidGap) {
  auto p = Provider();
  auto quadratic = [=](double t, ld::EphemerisSample* s, std::string* e) {
    return p((t * t - 400) / 30, s, e);
  };
  auto settings = Settings(false);
  settings.moon_contact = ld::DistanceContact::Center;
  settings.body_contact = ld::DistanceContact::Center;
  auto o = Observation(settings, quadratic, 20);
  auto gap = [=](double t, ld::EphemerisSample* s, std::string* e) {
    return std::fabs(t) > 5 && quadratic(t, s, e);
  };
  auto r = ld::SolveTimeTagged(o, gap, Options());
  ASSERT_TRUE(r.valid) << r.error;
  ASSERT_NE(Nearest(r, -20), nullptr);
  ASSERT_NE(Nearest(r, 20), nullptr);
  EXPECT_NEAR(Nearest(r, -20)->offset_seconds, -20, .0005);
  EXPECT_NEAR(Nearest(r, 20)->offset_seconds, 20, .0005);
  EXPECT_TRUE(Warning(r, "More than one"));
}
TEST(LunarBoundary, BoundedProbeDiscoversInteriorFeasibleIsland) {
  auto p = Provider(15);
  auto o = Observation(Settings(), p, 15);
  auto island = [=](double t, ld::EphemerisSample* s, std::string* e) {
    return t >= 10 && t <= 20 && p(t, s, e);
  };
  auto r = ld::SolveTime(o, island, Options(0, 30, 30));
  ASSERT_TRUE(r.valid) << r.error;
  ASSERT_NE(Nearest(r, 15), nullptr);
  EXPECT_DOUBLE_EQ(Nearest(r, 15)->offset_seconds, 15);
}
TEST(LunarBoundary, ProviderFailureAndNonfiniteForwardDirectionsNeverFallBack) {
  auto p = Provider();
  auto o = Observation(Settings(), p, 0);
  auto failed = [=](double t, ld::EphemerisSample* s, std::string* e) {
    p(t, s, e);
    s->observer_direction = [](double, double, double, bool, double*, double*,
                               double*) { return false; };
    return true;
  };
  auto r = ld::SolveTime(o, failed, Options());
  EXPECT_FALSE(r.valid);
  EXPECT_TRUE(r.candidates.empty());
  auto nan = [=](double t, ld::EphemerisSample* s, std::string* e) {
    p(t, s, e);
    s->moon_semidiameter_deg = std::numeric_limits<double>::quiet_NaN();
    return true;
  };
  r = ld::SolveTime(o, nan, Options());
  EXPECT_FALSE(r.valid);
  EXPECT_TRUE(r.candidates.empty());
}
TEST(LunarBoundary, InvalidAndExcessiveSearchInputsStopBeforeCallingProvider) {
  auto p = Provider();
  auto o = Observation(Settings(), p, 0);
  int calls = 0;
  auto counted = [&](double t, ld::EphemerisSample* s, std::string* e) {
    ++calls;
    return p(t, s, e);
  };
  for (double step : {0., -1., std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN(), 1e-8}) {
    auto options = Options();
    options.scan_step_seconds = step;
    EXPECT_FALSE(ld::SolveTime(o, counted, options).valid);
  }
  EXPECT_EQ(calls, 0);
}

TEST(LunarBoundary, ExactEpochCacheIsLocalToOneSolve) {
  auto p = Provider();
  auto o = Observation(Settings(), p, 0);
  std::map<double, int> calls;
  auto counted = [&](double t, ld::EphemerisSample* s, std::string* e) {
    ++calls[t];
    return p(t, s, e);
  };
  auto r = ld::SolveTime(o, counted, Options());
  ASSERT_TRUE(r.valid) << r.error;
  for (const auto& entry : calls) EXPECT_EQ(entry.second, 1);
  auto saved = calls;
  r = ld::SolveTime(o, counted, Options());
  ASSERT_TRUE(r.valid) << r.error;
  EXPECT_EQ(calls.size(), saved.size());
  for (const auto& entry : calls) EXPECT_EQ(entry.second, 2);
}

TEST(LunarBoundary, ConcurrentSolvesHaveIndependentCachesAndResults) {
  std::vector<std::future<bool>> jobs;
  for (int worker = 0; worker < 4; ++worker)
    jobs.push_back(std::async(std::launch::async, [worker] {
      for (int i = 0; i < 12; ++i) {
        const double root = .125 * (worker + i);
        auto p = Provider(root);
        auto o = Observation(Settings(), p, root);
        auto r = ld::SolveTime(o, p, Options());
        auto c = Nearest(r, root);
        if (!r.valid || !c || std::fabs(c->offset_seconds - root) >= .0005)
          return false;
      }
      return true;
    }));
  for (auto& job : jobs) EXPECT_TRUE(job.get());
}
TEST(LunarSphericalConvention, EveryLimbAndDistanceContactRoundTrips) {
  auto p = Provider();
  ld::EphemerisSample sample;
  std::string error;
  ASSERT_TRUE(p(0, &sample, &error));
  for (auto moon_limb : {ld::AltitudeLimb::Lower, ld::AltitudeLimb::Center,
                         ld::AltitudeLimb::Upper})
    for (auto body_limb : {ld::AltitudeLimb::Lower, ld::AltitudeLimb::Center,
                           ld::AltitudeLimb::Upper})
      for (auto moon_contact :
           {ld::DistanceContact::Near, ld::DistanceContact::Center,
            ld::DistanceContact::Far})
        for (auto body_contact :
             {ld::DistanceContact::Near, ld::DistanceContact::Center,
              ld::DistanceContact::Far})
          for (bool artificial : {false, true}) {
            auto settings = Settings(false);
            settings.separate_times = false;
            settings.artificial_horizon = artificial;
            settings.moon_altitude_limb = moon_limb;
            settings.body_altitude_limb = body_limb;
            settings.moon_contact = moon_contact;
            settings.body_contact = body_contact;
            auto o = Observation(settings, p, 0);
            auto c = ld::ClearDistance(o, sample);
            ASSERT_TRUE(c.valid) << c.error;
            EXPECT_NEAR(c.cleared_distance_deg, sample.predicted_distance_deg,
                        1e-10);
            auto center = settings;
            center.moon_altitude_limb = ld::AltitudeLimb::Center;
            auto center_o = Observation(center, p, 0);
            auto center_c = ld::ClearDistance(center_o, sample);
            ASSERT_TRUE(center_c.valid);
            EXPECT_NEAR(c.moon_topocentric_semidiameter_deg,
                        center_c.moon_topocentric_semidiameter_deg, 1e-13);
            EXPECT_NEAR(c.moon_distance_limb_correction_deg,
                        center_c.moon_distance_limb_correction_deg, 1e-13);
          }
}
TEST(LunarSphericalConvention,
     DirectEndpointRootDoesNotRequireEphemerisOutsideBracket) {
  auto p = Provider();
  auto settings = Settings(false);
  settings.separate_times = false;
  auto o = Observation(settings, p, 0);
  int outside = 0;
  auto limited = [&](double t, ld::EphemerisSample* s, std::string* e) {
    if (t < 0 || t > 60) {
      ++outside;
      return false;
    }
    return p(t, s, e);
  };
  auto r = ld::SolveTime(o, limited, Options(0, 60));
  ASSERT_TRUE(r.valid) << r.error;
  ASSERT_NE(Nearest(r, 0), nullptr);
  EXPECT_NEAR(Nearest(r, 0)->offset_seconds, 0, .0005);
  EXPECT_TRUE(Nearest(r, 0)->local_slope_available);
  EXPECT_EQ(outside, 0);
}

TEST(LunarBoundary, DirectScanTerminatesWhenStepIsBelowEpochPrecision) {
  auto p = Provider();
  auto settings = Settings(false);
  settings.separate_times = false;
  auto o = Observation(settings, p, 0);
  ld::EphemerisSample sample;
  std::string error;
  ASSERT_TRUE(p(0, &sample, &error));
  auto clearance = ld::ClearDistance(o, sample);
  ASSERT_TRUE(clearance.valid);
  sample.predicted_distance_deg = clearance.cleared_distance_deg + 1;
  int calls = 0;
  auto constant = [&](double, ld::EphemerisSample* s, std::string*) {
    ++calls;
    *s = sample;
    return true;
  };
  auto options = Options(1e15, 1e15 + 1, .0001);
  EXPECT_FALSE(ld::SolveTime(o, constant, options).valid);
  EXPECT_LE(calls, 16);
}
