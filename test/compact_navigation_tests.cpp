#include <gtest/gtest.h>
#include "CompactEphemerisProvider.h"
#include "NavigationAlgorithms.h"
#include "Sight.h"
#include "LunarSolutionRecord.h"
#include "UtcDateTime.h"
#include "lunar_reference_support.hpp"
#include "celestial_navigation_pi.h"
#include <wx/checkbox.h>
#include <wx/fileconf.h>
#include <wx/utils.h>
#include <cmath>
#include <atomic>
#include <thread>

namespace {
struct CompactScope {
  wxFileConfig* config = GetOCPNConfigObject();
  bool old = true;
  bool existed =
      config->Read("/PlugIns/CelestialNavigation/UseCompactEphemeris", &old);
  wxString enable;
  bool hadEnable = wxGetEnv("CELNAV_TEST_DE440_ENABLE", &enable);
  wxString path;
  bool hadPath = wxGetEnv("CELNAV_TEST_DE440_PATH", &path);
  CompactScope() {
    wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
    config->DeleteEntry("/PlugIns/CelestialNavigation/UseCompactEphemeris");
    wxSetEnv("CELNAV_TEST_DE440_PATH", "/unavailable/celnav/de440s.bsp");
  }
  ~CompactScope() {
    if (existed)
      config->Write("/PlugIns/CelestialNavigation/UseCompactEphemeris", old);
    else
      config->DeleteEntry("/PlugIns/CelestialNavigation/UseCompactEphemeris");
    if (hadPath)
      wxSetEnv("CELNAV_TEST_DE440_PATH", path);
    else
      wxUnsetEnv("CELNAV_TEST_DE440_PATH");
    if (hadEnable)
      wxSetEnv("CELNAV_TEST_DE440_ENABLE", enable);
    else
      wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
    wxUnsetEnv("CELNAV_TEST_COMPACT_PATH");
  }
};
wxDateTime UtcFields(const char* iso) {
  wxDateTime t;
  t.ParseISOCombined(iso);
  return t;
}
}  // namespace
TEST(CompactNavigation, DefaultEnabledAndDisableRetainsClassic) {
  CompactScope scope;
  EXPECT_TRUE(celestial_navigation::CompactEphemerisEnabled());
  const auto t = UtcDateTime::ToInstant(UtcFields("2024-06-13T19:26:00"));
  const auto modern = CelestialEphemeris::Evaluate("Moon", t, 41, -71);
  ASSERT_TRUE(modern.valid);
  EXPECT_TRUE(modern.usedCompact);
  EXPECT_FALSE(modern.usedDe440);
  celestial_navigation::SetCompactEphemerisEnabled(false);
  EXPECT_FALSE(celestial_navigation::CompactEphemerisEnabled());
  const auto classic = CelestialEphemeris::Evaluate("Moon", t, 41, -71);
  ASSERT_TRUE(classic.valid);
  EXPECT_FALSE(classic.usedCompact);
  EXPECT_FALSE(classic.usedDe440);
  EXPECT_GT(std::fabs(modern.declination - classic.declination), 1e-7);
}
TEST(CompactNavigation, AllSupportedBodiesShareSightAndPlannerCoordinates) {
  CompactScope scope;
  auto engine = celestial_navigation::CompactEngine();
  ASSERT_TRUE(engine);
  const auto fields = UtcFields("2003-03-11T12:00:00");
  const auto instant = UtcDateTime::ToInstant(fields);
  for (const auto& name : engine->Bodies()) {
    if (name == "Aries") continue;
    const auto body = wxString::FromUTF8(name.c_str());
    Sight sight(Sight::ALTITUDE, body, Sight::CENTER, fields, 0, 30, .2);
    double lat = NAN, lon = NAN;
    bool de = false, compact = false;
    sight.BodyLocation(fields, &lat, &lon, nullptr, nullptr, nullptr, false,
                       true, NAN, &de, &compact);
    ASSERT_TRUE(compact) << name;
    EXPECT_FALSE(de);
    const auto state =
        CelestialEphemeris::Evaluate(body, instant, 28.75, -41.125);
    ASSERT_TRUE(state.valid) << name;
    EXPECT_TRUE(state.usedCompact);
    EXPECT_DOUBLE_EQ(lat, state.declination);
    EXPECT_NEAR(std::remainder(-lon - state.gha, 360.0), 0, 1e-12);
    EXPECT_TRUE(std::isfinite(state.apparentAltitude));
    EXPECT_TRUE(std::isfinite(state.azimuthTrue));
  }
}
TEST(CompactNavigation, MissingDataFallsBackAndRecovers) {
  CompactScope scope;
  const auto t = UtcDateTime::ToInstant(UtcFields("2024-06-13T19:26:00"));
  wxSetEnv("CELNAV_TEST_COMPACT_PATH", "/unavailable/celnav/compact");
  const auto classic = CelestialEphemeris::Evaluate("Moon", t, 0, 0);
  ASSERT_TRUE(classic.valid);
  EXPECT_FALSE(classic.usedCompact);
  wxUnsetEnv("CELNAV_TEST_COMPACT_PATH");
  const auto modern = CelestialEphemeris::Evaluate("Moon", t, 0, 0);
  ASSERT_TRUE(modern.valid);
  EXPECT_TRUE(modern.usedCompact);
}
TEST(CompactNavigation, WholeIntervalCoverageAndFrozenSetting) {
  CompactScope scope;
  const auto epoch = UtcFields("2100-12-31T23:59:00");
  auto update = eclipse::GetDut1Update();
  auto outside = celestial_navigation::SelectEnhancedLunarProvider(
      "Sun", epoch, -120, 120, false, true, update);
  EXPECT_FALSE(outside.ephemeris);
  EXPECT_NE(outside.fallback_reason.find("1972"), std::string::npos);
  auto session = celestial_navigation::SelectEnhancedLunarProvider(
      "Sun", UtcFields("2024-06-13T19:26:00"), -120, 120, false, true, update);
  ASSERT_TRUE(session.ephemeris);
  EXPECT_TRUE(session.used_compact);
  celestial_navigation::SetCompactEphemerisEnabled(false);
  lunar_distance::EphemerisSample sample;
  std::string error;
  EXPECT_TRUE(session.ephemeris(0, &sample, &error)) << error;
  // Calls beyond the selected model's coverage report failure; no silent
  // classic switch.
  EXPECT_FALSE(session.ephemeris(100 * 365.25 * 86400, &sample, &error));
  EXPECT_FALSE(error.empty());
}
TEST(CompactNavigation, KernelRetainsPriorityAndStarsUseCompact) {
  CompactScope scope;
  wxSetEnv("CELNAV_TEST_DE440_PATH",
           wxString::FromUTF8(ECLIPSE_DE440_TEST_PATH));
  wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
  struct Unset {
    ~Unset() { wxUnsetEnv("CELNAV_TEST_DE440_ENABLE"); }
  } unset;
  const auto fields = UtcFields("2024-06-13T19:26:00");
  const auto instant = UtcDateTime::ToInstant(fields);
  const auto sun = CelestialEphemeris::Evaluate("Sun", instant, 41, -71);
  ASSERT_TRUE(sun.valid);
  EXPECT_TRUE(sun.usedDe440);
  EXPECT_FALSE(sun.usedCompact);
  const auto star = CelestialEphemeris::Evaluate("Sirius", instant, 41, -71);
  ASSERT_TRUE(star.valid);
  EXPECT_TRUE(star.usedCompact);
  EXPECT_FALSE(star.usedDe440);
  const auto session = celestial_navigation::SelectEnhancedLunarProvider(
      "Sun", fields, -120, 120, true, true, eclipse::GetDut1Update());
  ASSERT_TRUE(session.ephemeris);
  EXPECT_TRUE(session.used_de440);
  EXPECT_FALSE(session.used_compact);
}
TEST(CompactNavigation, DatesOutsideCoverageRetainClassic) {
  CompactScope scope;
  for (const char* iso : {"1971-12-31T12:00:00", "2101-01-01T12:00:00"}) {
    const auto state = CelestialEphemeris::Evaluate(
        "Sun", UtcDateTime::ToInstant(UtcFields(iso)), 28, -41);
    ASSERT_TRUE(state.valid) << iso;
    EXPECT_FALSE(state.usedCompact);
  }
}
TEST(CompactNavigation, IndependentLunarReferencesThroughProductionSight) {
  CompactScope scope;
  const auto cases = Cases();
  ASSERT_EQ(cases.size(), 32u);
  std::map<std::string, std::string> utc;
  std::ifstream input(std::string(LUNAR_REFERENCE_DIR) +
                      "/utc-consistent/cases.tsv");
  std::string line;
  std::getline(input, line);
  while (std::getline(input, line)) {
    const auto row = Fields(line);
    utc[row[0]] = row[1];
  }
  double largest = 0;
  for (const auto& c : cases)
    for (bool tagged : {false, true}) {
      SCOPED_TRACE(c.name + (tagged ? " sequential/moving" : " simultaneous"));
      const auto reference = Reference(c);
      lunar_distance::Observation o;
      o.use_ellipsoid = true;
      o.separate_times = tagged;
      o.moving_observer = tagged;
      o.moon_time_offset_seconds = tagged ? -90 : 0;
      o.body_time_offset_seconds = tagged ? 120 : 0;
      o.course_true_deg = 80;
      o.speed_knots = 7;
      o.moon_altitude_limb = lunar_distance::AltitudeLimb::Lower;
      o.body_altitude_limb = lunar_distance::AltitudeLimb::Lower;
      o.moon_contact = lunar_distance::DistanceContact::Near;
      o.body_contact = c.body == "Sun"
                           ? lunar_distance::DistanceContact::Near
                           : lunar_distance::DistanceContact::Center;
      o.eye_height_m = 2;
      o.pressure_hpa = 1010;
      o.temperature_c = 10;
      auto forward = lunar_distance::PredictTimeTaggedObservation(
          o, reference, 0, c.observer);
      ASSERT_TRUE(forward.valid) << c.name << ' ' << forward.error;
      const auto fields = UtcFields(utc[c.name].c_str());
      Sight sight(Sight::LUNAR, wxString::FromUTF8(c.body.c_str()),
                  Sight::LUNAR_NEAR, fields, 240, forward.raw_distance_deg,
                  .02);
      sight.m_EyeHeight = 2;
      sight.m_Pressure = 1010;
      sight.m_Temperature = 10;
      sight.m_IndexError = 0;
      sight.m_LunarMoonAltitude = forward.moon_altitude_deg;
      sight.m_LunarBodyAltitude = forward.body_altitude_deg;
      sight.m_LunarMoonLimb = Sight::LOWER;
      sight.m_LunarBodyLimb = Sight::LOWER;
      sight.m_LunarBodyDistanceLimb = Sight::LUNAR_NEAR;
      sight.m_DRLat = c.observer.latitude_deg;
      sight.m_DRLon = c.observer.longitude_deg;
      sight.m_AllowDe440 = false;
      sight.m_LunarSeparateTimes = tagged;
      sight.m_LunarMovingObserver = tagged;
      sight.m_LunarMoonTimeOffsetSeconds = tagged ? -90 : 0;
      sight.m_LunarBodyTimeOffsetSeconds = tagged ? 120 : 0;
      sight.m_LunarCourseTrue = 80;
      sight.m_LunarSpeedKnots = 7;
      sight.m_LunarMoonAltitudeUncertainty = .02;
      sight.m_LunarBodyAltitudeUncertainty = .02;
      sight.Recompute(0);
      ASSERT_TRUE(sight.m_LunarUsesCompact) << c.name;
      ASSERT_TRUE(sight.m_LunarSolutionValid)
          << c.name << ' ' << sight.m_LunarSolutionError;
      ASSERT_GE(sight.m_LunarSelectedCandidate, 0) << c.name;
      const auto& candidate =
          sight.m_LunarCandidates[sight.m_LunarSelectedCandidate];
      largest = std::max(largest, std::fabs(candidate.offset_seconds));
      EXPECT_LT(std::fabs(candidate.offset_seconds), 2.0) << c.name;
      EXPECT_NE(LunarInputSnapshot(sight).Find("compact-0.2.0"), wxNOT_FOUND);
      EXPECT_NE(sight.m_CalcStr.Find("Compact 0.2.0"), wxNOT_FOUND);
    }
  std::cout << "Production compact lunar maximum UTC error: " << largest
            << " s\n";
}
TEST(CompactNavigation, SavedUnavailableUncertaintyRoundTrips) {
  LunarSolutionRecord r;
  r.name = "Boundary";
  r.time_sigma_seconds = INFINITY;
  EXPECT_NE(r.Details().Find("unavailable"), wxNOT_FOUND);
  EXPECT_EQ(r.Details().Find("inf"), wxNOT_FOUND);
  TiXmlElement clock("Clock");
  WriteLunarSolutions(&clock, {r});
  const auto records = ReadLunarSolutions(&clock);
  ASSERT_EQ(records.size(), 1u);
  EXPECT_TRUE(std::isinf(records[0].time_sigma_seconds));
}

TEST(CompactNavigation, RealInstantPreservesUtcAtDstTransitions) {
  CompactScope scope;
  for (const auto date : {wxDateTime(8, wxDateTime::Mar, 2026, 12),
                          wxDateTime(29, wxDateTime::Mar, 2026, 12),
                          wxDateTime(2, wxDateTime::Nov, 2025, 12)}) {
    const auto instant = UtcDateTime::ToInstant(date) - wxTimeSpan::Hours(10) +
                         wxTimeSpan::Minutes(30);
    const auto utc = celestial_navigation::CompactUtc(instant, true);
    const auto request = celestial_navigation::CompactRequest(
        "Moon", instant, NAN, eclipse::GetDut1Update(), true);
    const auto independent =
        celestial_navigation::CompactEngine()->Evaluate(request);
    EXPECT_NEAR(independent.epoch.utc_jd, instant.GetJulianDayNumber(), 1e-9);
    const auto state = CelestialEphemeris::Evaluate("Moon", instant, 28, -41);
    ASSERT_TRUE(state.valid);
    ASSERT_TRUE(state.usedCompact);
    EXPECT_DOUBLE_EQ(state.gha, independent.gha_deg) << utc;
    EXPECT_DOUBLE_EQ(state.declination, independent.declination_deg) << utc;
    Sight sight(Sight::ALTITUDE, "Moon", Sight::CENTER, date, 0, 30, .2);
    double lat = NAN, lon = NAN;
    bool compact = false;
    sight.BodyLocation(instant, &lat, &lon, nullptr, nullptr, nullptr, true,
                       false, NAN, nullptr, &compact);
    ASSERT_TRUE(compact);
    EXPECT_DOUBLE_EQ(lat, independent.declination_deg);
    EXPECT_NEAR(std::remainder(-lon - independent.gha_deg, 360.0), 0, 1e-12);
    wxSetEnv("CELNAV_TEST_DE440_PATH",
             wxString::FromUTF8(ECLIPSE_DE440_TEST_PATH));
    wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
    const auto kernel = CelestialEphemeris::Evaluate("Moon", instant, 28, -41);
    ASSERT_TRUE(kernel.valid);
    ASSERT_TRUE(kernel.usedDe440);
    // Separate validated theories agree to sub-arcsecond precision. A local
    // DST-hour shift would exceed these bounds by orders of magnitude.
    EXPECT_NEAR(std::remainder(kernel.gha - independent.gha_deg, 360.0), 0,
                1.0 / 3600);
    EXPECT_NEAR(kernel.declination, independent.declination_deg, 1.0 / 3600);
    wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
    wxSetEnv("CELNAV_TEST_DE440_PATH", "/unavailable/celnav/de440s.bsp");
  }
}
TEST(CompactNavigation, LunarCallbackKeepsUtcAcrossLocalDstGap) {
  CompactScope scope;
  const wxDateTime reference(8, wxDateTime::Mar, 2026, 0, 30, 0);
  const auto update = eclipse::GetDut1Update();
  const auto selected = celestial_navigation::SelectEnhancedLunarProvider(
      "Sun", reference, 0, 10800, false, true, update);
  ASSERT_TRUE(selected.used_compact);
  ASSERT_TRUE(selected.ephemeris);
  lunar_distance::EphemerisSample sample;
  std::string error;
  ASSERT_TRUE(selected.ephemeris(7200, &sample, &error)) << error;
  const auto instant = UtcDateTime::ToInstant(reference) + wxTimeSpan::Hours(2);
  const auto request =
      celestial_navigation::CompactRequest("Moon", instant, NAN, update, true);
  EXPECT_EQ(request.utc, "2026-03-08T02:30:00.000");
  const auto moon = celestial_navigation::CompactEngine()->Evaluate(request);
  EXPECT_DOUBLE_EQ(sample.moon_geographic_latitude_deg, moon.declination_deg);
  EXPECT_NEAR(std::remainder(
                  -sample.moon_geographic_longitude_deg - moon.gha_deg, 360.0),
              0, 1e-12);
}

// Independent callers share exact epoch/result caches safely.
TEST(CompactNavigation, ConcurrentEpochCacheMatchesUncachedResults) {
  CompactScope scope;
  const auto engine = celestial_navigation::CompactEngine();
  ASSERT_TRUE(engine);
  celnav::Options options;
  options.data_directory = CELNAV_COMPACT_TEST_DATA;
  options.cache_epoch_context = false;
  const celnav::Engine uncached(options);
  std::vector<celnav::Request> requests;
  std::vector<celnav::Result> expected;
  const char* bodies[] = {"Moon", "Sun", "Sirius", "Venus", "Jupiter"};
  for (int i = 0; i < 24; ++i) {
    celnav::Request request;
    request.body = bodies[i % 5];
    request.utc = "2024-06-13T" + (i < 10 ? std::string("0") : std::string()) +
                  std::to_string(i) + ":26:00.125";
    request.latitude_deg = 41;
    request.longitude_deg = -71;
    request.venus_phase = request.body == "Venus";
    expected.push_back(uncached.Evaluate(request));
    requests.push_back(request);
  }
  std::atomic_bool start{false};
  std::vector<std::thread> workers;
  for (int worker = 0; worker < 6; ++worker) {
    workers.emplace_back([&, worker] {
      while (!start.load()) std::this_thread::yield();
      for (int i = 0; i < 48; ++i) {
        const auto index = (i + worker * 5) % requests.size();
        const auto actual = engine->Evaluate(requests[index]);
        EXPECT_DOUBLE_EQ(actual.gha_deg, expected[index].gha_deg);
        EXPECT_DOUBLE_EQ(actual.declination_deg, expected[index].declination_deg);
        EXPECT_DOUBLE_EQ(actual.distance_km, expected[index].distance_km);
        EXPECT_DOUBLE_EQ(actual.airless_altitude_deg,
                         expected[index].airless_altitude_deg);
      }
    });
  }
  start.store(true);
  for (auto& worker : workers) worker.join();
}

TEST(CompactNavigation, ConcurrentRetainedLunarProvidersMatchSerialSamples) {
  CompactScope scope;
  wxSetEnv("CELNAV_TEST_DE440_PATH",
           wxString::FromUTF8(ECLIPSE_DE440_TEST_PATH));
  const auto fields = UtcFields("2024-06-13T19:26:00");
  const auto update = eclipse::GetDut1Update();
  std::vector<lunar_distance::EphemerisFunction> providers;
  for (const bool de : {false, true}) {
    auto selected = celestial_navigation::SelectEnhancedLunarProvider(
        "Sun", fields, -120, 120, de, true, update);
    ASSERT_TRUE(selected.ephemeris);
    EXPECT_EQ(selected.used_de440, de);
    EXPECT_EQ(selected.used_compact, !de);
    providers.push_back(selected.ephemeris);
  }
  Sight classic(Sight::LUNAR, "Sun", Sight::LUNAR_NEAR, fields, 240, 85, .2);
  classic.m_AllowDe440 = false;
  classic.m_AllowCompact = false;
  classic.Recompute(0);
  ASSERT_TRUE(classic.LunarEphemeris());
  providers.push_back(classic.LunarEphemeris());
  for (const auto& provider : providers) {
    std::vector<lunar_distance::EphemerisSample> expected(9);
    std::string error;
    for (int i = 0; i < 9; ++i)
      ASSERT_TRUE(provider(i * 30 - 120, &expected[i], &error)) << error;
    std::atomic_bool start{false};
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker) {
      workers.emplace_back([&, worker] {
        while (!start.load()) std::this_thread::yield();
        for (int i = 0; i < 18; ++i) {
          const auto index = (i + worker) % expected.size();
          lunar_distance::EphemerisSample actual;
          std::string reason;
          ASSERT_TRUE(provider(index * 30.0 - 120, &actual, &reason)) << reason;
          EXPECT_DOUBLE_EQ(actual.predicted_distance_deg,
                           expected[index].predicted_distance_deg);
          EXPECT_DOUBLE_EQ(actual.moon_geographic_latitude_deg,
                           expected[index].moon_geographic_latitude_deg);
          EXPECT_DOUBLE_EQ(actual.body_geographic_longitude_deg,
                           expected[index].body_geographic_longitude_deg);
          if (actual.observer_direction) {
            double alt, az, sd, expected_alt, expected_az, expected_sd;
            ASSERT_TRUE(actual.observer_direction(41, -71, 2, true,
                                                   &alt, &az, &sd));
            ASSERT_TRUE(expected[index].observer_direction(41, -71, 2, true,
                     &expected_alt, &expected_az, &expected_sd));
            EXPECT_DOUBLE_EQ(alt, expected_alt);
            EXPECT_DOUBLE_EQ(az, expected_az);
            EXPECT_DOUBLE_EQ(sd, expected_sd);
          }
        }
      });
    }
    start.store(true);
    for (auto& worker : workers) worker.join();
  }
}
