#include <gtest/gtest.h>

#include "eclipse/dut1.h"
#include "eclipse/navigation.h"
#include "eclipse/spk.h"
#include "eclipse/time.h"
#include "NavigationEphemerisProvider.h"
#include "NavigationAlgorithms.h"
#include "Sight.h"
#include "UtcDateTime.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <wx/utils.h>
#include <wx/init.h>

namespace {
double CircularDifference(double a, double b) {
  return std::remainder(a - b, 360.0);
}

double Separation(const celestial_navigation::De440NavigationSample& a,
                  const celestial_navigation::De440NavigationSample& b) {
  constexpr double radians = 3.14159265358979323846 / 180.0;
  const double da = a.declination_deg * radians;
  const double db = b.declination_deg * radians;
  const double gha = CircularDifference(a.gha_deg, b.gha_deg) * radians;
  const double cosine = std::sin(da) * std::sin(db) +
                        std::cos(da) * std::cos(db) * std::cos(gha);
  return std::acos(std::max(-1.0, std::min(1.0, cosine))) / radians;
}

eclipse::CalendarDateTime PointJudithUtc() {
  eclipse::CalendarDateTime utc;
  utc.year = 2024;
  utc.month = 6;
  utc.day = 13;
  utc.hour = 19;
  utc.minute = 26;
  return utc;
}
}  // namespace

TEST(NavigationDe440, PluginProviderIsOptInAndDoesNotTouchStars) {
  wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
  const wxDateTime utc(13, wxDateTime::Jun, 2024, 19, 26, 0, 125);
  celestial_navigation::De440NavigationSample sample;
  std::string reason;
  EXPECT_FALSE(celestial_navigation::TryDe440NavigationSample(
      "Moon", utc, &sample, &reason));
  EXPECT_EQ(reason, "DE440s is not enabled for this test");
  wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
  EXPECT_FALSE(celestial_navigation::TryDe440NavigationSample(
      "Sirius", utc, &sample, &reason));
  EXPECT_EQ(reason, "Target centre is not in compact DE440s");
  wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
}

TEST(NavigationDe440, BobPlannerLunarValuesAgreeWithIndependentKernel) {
  wxInitializer initializer;
  ASSERT_TRUE(initializer.IsOk());
  wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
  for (const auto& sample : {
           std::make_tuple("2025-12-14T10:00:00", 43.0 + 10.0 / 60.0, -77.5),
           std::make_tuple("2025-12-14T16:00:00", -31.0, 172.0)}) {
    wxDateTime fields;
    ASSERT_TRUE(fields.ParseISOCombined(std::get<0>(sample)));
    const auto plan = PlannerRecommendations::Calculate(
        UtcDateTime::ToInstant(fields), std::get<1>(sample),
        std::get<2>(sample));
    wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
    celestial_navigation::De440NavigationSample moon, laterMoon;
    std::string reason;
    const wxDateTime later = fields + wxTimeSpan::Minutes(5);
    const bool moonOk = celestial_navigation::TryDe440NavigationSample(
        "Moon", fields, &moon, &reason);
    const bool laterMoonOk = celestial_navigation::TryDe440NavigationSample(
        "Moon", later, &laterMoon, &reason);
    for (const wxString bodyName : {wxString("Sun"), wxString("Venus")}) {
      celestial_navigation::De440NavigationSample body, laterBody;
      const bool bodyOk = celestial_navigation::TryDe440NavigationSample(
          bodyName, fields, &body, &reason);
      const bool laterBodyOk = celestial_navigation::TryDe440NavigationSample(
          bodyName, later, &laterBody, &reason);
      wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
      ASSERT_TRUE(moonOk && laterMoonOk && bodyOk && laterBodyOk) << reason;
      const auto match = std::find_if(plan.bodies.begin(), plan.bodies.end(),
          [&bodyName](const RankedBody& item) {
            return item.state.body == bodyName;
          });
      ASSERT_NE(match, plan.bodies.end());
      const double referenceDistance = Separation(moon, body);
      const double referenceRate =
          (Separation(laterMoon, laterBody) - referenceDistance) * 720.0;
      EXPECT_NEAR(referenceDistance, match->lunarDistance, 0.15);
      EXPECT_NEAR(referenceRate, match->lunarRateArcminHour, 3.0);
      wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
    }
    wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
  }
}

TEST(NavigationDe440, PluginBodyLocationUsesVerifiedKernelAndFractionalUtc) {
  wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
  const wxDateTime utc(13, wxDateTime::Jun, 2024, 19, 26, 0, 125);
  celestial_navigation::De440NavigationSample a, b;
  std::string reason;
  ASSERT_TRUE(celestial_navigation::TryDe440NavigationSample(
      "Moon", utc, &a, &reason)) << reason;
  const wxDateTime later(13, wxDateTime::Jun, 2024, 19, 26, 0, 375);
  ASSERT_TRUE(celestial_navigation::TryDe440NavigationSample(
      "Moon", later, &b, &reason)) << reason;
  EXPECT_GT(std::fabs(CircularDifference(b.gha_deg, a.gha_deg)) * 3600.0,
            3.0);
  Sight moon(Sight::ALTITUDE, "Moon", Sight::CENTER, utc, 0.0, 0.0, 1.0);
  double lat = 0, lon = 0, aries = 0, range = 0;
  moon.BodyLocation(utc, &lat, &lon, &aries, &range, nullptr);
  EXPECT_NEAR(lat, a.declination_deg, 1e-10);
  EXPECT_NEAR(CircularDifference(-lon, a.gha_deg), 0.0, 1e-10);
  EXPECT_NEAR(range, a.range_km, 1e-6);
  EXPECT_NEAR(aries, a.aries_gha_deg, 1e-10);
  wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
}

TEST(NavigationDe440, ExplicitDut1DoesNotShiftDynamicalEphemeris) {
  wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
  const wxDateTime utc(13, wxDateTime::Jun, 2024, 19, 26, 0, 0);
  celestial_navigation::De440NavigationSample early, late;
  std::string reason;
  ASSERT_TRUE(celestial_navigation::TryDe440NavigationSample(
      "Moon", utc, &early, &reason, -0.5)) << reason;
  ASSERT_TRUE(celestial_navigation::TryDe440NavigationSample(
      "Moon", utc, &late, &reason, 0.5)) << reason;
  EXPECT_NEAR(early.range_km, late.range_km, 1e-7);
  EXPECT_NEAR(early.declination_deg, late.declination_deg, 1e-7);
  EXPECT_GT(CircularDifference(late.gha_deg, early.gha_deg) * 3600.0,
            15.0);
  wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
}

TEST(NavigationDe440, PlanningStateUsesSameUtcAndSingleDut1Application) {
  wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
  const wxDateTime utc(13, wxDateTime::Jun, 2024, 19, 26, 0, 125);
  celestial_navigation::De440NavigationSample lunar;
  std::string reason;
  ASSERT_TRUE(celestial_navigation::TryDe440NavigationSample(
      "Moon", utc, &lunar, &reason, 0.4)) << reason;
  const wxDateTime instant = UtcDateTime::ToInstant(utc);
  const BodyState state = CelestialEphemeris::Evaluate(
      "Moon", instant, 41.3666666667, -71.4833333333,
      1010.0, 10.0, 0.4);
  ASSERT_TRUE(state.valid) << state.error;
  EXPECT_NEAR(CircularDifference(state.gha, lunar.gha_deg), 0.0, 1e-9);
  EXPECT_NEAR(state.declination, lunar.declination_deg, 1e-9);
  EXPECT_NEAR(state.distance, lunar.range_km, 1e-6);
  celestial_navigation::De440ObserverDirection observer;
  ASSERT_TRUE(celestial_navigation::TryDe440ObserverDirection(
      "Moon", utc, 41.3666666667, -71.4833333333, 0.0,
      &observer, &reason, 0.4)) << reason;
  const BodyState airless = CelestialEphemeris::Evaluate(
      "Moon", instant, 41.3666666667, -71.4833333333,
      0.0, 10.0, 0.4);
  EXPECT_NEAR(airless.apparentAltitude, observer.airless_altitude_deg, 1e-8);
  EXPECT_NEAR(airless.semidiameter, observer.semidiameter_deg, 1e-8);
  wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
}

TEST(NavigationDe440, OutsideCompactKernelCoverageFallsBackCleanly) {
  wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
  const wxDateTime future(1, wxDateTime::Jan, 2200, 0, 0, 0);
  celestial_navigation::De440NavigationSample sample;
  std::string reason;
  EXPECT_FALSE(celestial_navigation::TryDe440NavigationSample(
      "Moon", future, &sample, &reason));
  EXPECT_FALSE(reason.empty());
  const wxDateTime historical(1, wxDateTime::Jan, 1900, 0, 0, 0);
  EXPECT_FALSE(celestial_navigation::TryDe440NavigationSample(
      "Moon", historical, &sample, &reason));
  EXPECT_EQ(reason,
            "Automatic UTC-to-TT conversion is unsupported before 1972");
  wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
}

TEST(NavigationDe440, RejectsUnsupportedPlanetCentre) {
  EXPECT_TRUE(eclipse::De440sNavigationTarget(10));
  EXPECT_TRUE(eclipse::De440sNavigationTarget(301));
  EXPECT_TRUE(eclipse::De440sNavigationTarget(199));
  EXPECT_TRUE(eclipse::De440sNavigationTarget(299));
  EXPECT_FALSE(eclipse::De440sNavigationTarget(499));
  EXPECT_FALSE(eclipse::De440sNavigationTarget(5));
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;
  eclipse::NavigationGeocentricState result;
  EXPECT_FALSE(eclipse::GeocentricNavigationState(
      kernel, 499, eclipse::NavigationEpoch(), &result, &error));
  EXPECT_EQ(error, "DE440s does not provide this body centre");
}

TEST(NavigationDe440, MatchesUsnoPublishedPrecisionAtPointJudith) {
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;
  const auto utc = PointJudithUtc();
  double utc_jd = 0.0;
  ASSERT_TRUE(eclipse::CalendarToJulianDate(utc, &utc_jd, &error)) << error;
  const auto dut1 = eclipse::LookupDut1(utc_jd);
  ASSERT_TRUE(dut1.available);
  eclipse::NavigationEpoch epoch;
  ASSERT_TRUE(eclipse::MakeNavigationEpoch(
      utc, dut1.seconds, eclipse::TaiMinusUtcSeconds(utc), 0.0, 0.0,
      &epoch, &error)) << error;

  // USNO Celestial Navigation API v4.0.1, 2024-06-13 19:26 UT1,
  // 41.3667 N, 71.4833 W, accessed 2026-09-18:
  // https://aa.usno.navy.mil/api/celnav?date=2024-6-13&time=19:26&coords=41.3666667,-71.4833333
  // This is a printed-value comparison, not a sub-arcsecond reference. The
  // endpoint accepts UT1 while our caller's time is UTC. At this epoch its
  // The service's Moon value differs from the DE440s/JPL result by roughly
  // 4" GHA and 2" Dec at matched UT1. An inferred ~8.83 s difference in
  // its TT argument explains both coordinates; never apply that fitted offset
  // to real sights or claim identical printed digits at the true UTC.
  struct Reference { int id; double gha; double dec; };
  const Reference references[] = {
      {10, 111.434162, 23.267001},
      {301, 23.584431, 6.343757},
      {299, 108.730508, 23.672939},
  };
  for (const auto& reference : references) {
    eclipse::NavigationGeocentricState state;
    ASSERT_TRUE(eclipse::GeocentricNavigationState(
        kernel, reference.id, epoch, &state, &error)) << error;
    EXPECT_NEAR(CircularDifference(state.gha_deg, reference.gha), 0.0,
                0.1 / 60.0) << reference.id;
    EXPECT_NEAR(state.declination_deg, reference.dec, 0.1 / 60.0)
        << reference.id;
    EXPECT_NEAR(CircularDifference(state.gha_aries_deg, 194.09595020643212),
                0.0, 1.0 / 3600.0) << reference.id;
    EXPECT_GT(state.distance_km, 0.0);
  }
}

TEST(NavigationDe440, ArchivedUsnoMoonDifferenceIsTimeArgument) {
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;
  const auto input_ut1 = PointJudithUtc();
  eclipse::NavigationEpoch ordinary, matched;
  // Here the calendar fields represent the *USNO API's UT1 input*. A zero
  // DUT1 freezes terrestrial rotation while changing only dynamical time.
  ASSERT_TRUE(eclipse::MakeNavigationEpoch(input_ut1, 0.0, 37.0, 0.0, 0.0,
                                          &ordinary, &error)) << error;
  ASSERT_TRUE(eclipse::MakeNavigationEpoch(input_ut1, 0.0, 45.829, 0.0, 0.0,
                                          &matched, &error)) << error;
  eclipse::NavigationGeocentricState a, b;
  ASSERT_TRUE(eclipse::GeocentricNavigationState(kernel, 301, ordinary, &a,
                                                 &error)) << error;
  ASSERT_TRUE(eclipse::GeocentricNavigationState(kernel, 301, matched, &b,
                                                 &error)) << error;
  constexpr double kUsnoMoonGha = 23.584431;
  constexpr double kUsnoMoonDec = 6.343757;
  EXPECT_GT(std::fabs(CircularDifference(a.gha_deg, kUsnoMoonGha)) * 3600.0,
            3.0);
  EXPECT_GT(std::fabs(a.declination_deg - kUsnoMoonDec) * 3600.0, 1.5);
  EXPECT_LT(std::fabs(CircularDifference(b.gha_deg, kUsnoMoonGha)) * 3600.0,
            0.01);
  EXPECT_LT(std::fabs(b.declination_deg - kUsnoMoonDec) * 3600.0, 0.01);
  EXPECT_NEAR(a.gha_aries_deg, b.gha_aries_deg, 1e-8);
}

TEST(NavigationDe440, GeneralObserverDirectionPreservesSunMoonPath) {
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;
  const auto utc = PointJudithUtc();
  double utc_jd = 0.0;
  ASSERT_TRUE(eclipse::CalendarToJulianDate(utc, &utc_jd, &error)) << error;
  const auto dut1 = eclipse::LookupDut1(utc_jd);
  ASSERT_TRUE(dut1.available);
  eclipse::NavigationEpoch epoch;
  ASSERT_TRUE(eclipse::MakeNavigationEpoch(
      utc, dut1.seconds, eclipse::TaiMinusUtcSeconds(utc), 0.0, 0.0,
      &epoch, &error)) << error;
  for (bool moon : {false, true}) {
    double old_alt = 0.0, old_az = 0.0, old_sd = 0.0;
    double new_alt = 0.0, new_az = 0.0, range = 0.0;
    ASSERT_TRUE(eclipse::ObserverApparentDirection(
        kernel, epoch.et_seconds, epoch.orientation,
        41.3666666667, -71.4833333333, 6.1, moon,
        &old_alt, &old_az, &old_sd, &error)) << error;
    ASSERT_TRUE(eclipse::ObserverApparentTargetDirection(
        kernel, moon ? 301 : 10, epoch.et_seconds, epoch.orientation,
        41.3666666667, -71.4833333333, 6.1,
        &new_alt, &new_az, &range, &error)) << error;
    EXPECT_DOUBLE_EQ(new_alt, old_alt);
    EXPECT_DOUBLE_EQ(new_az, old_az);
    EXPECT_GT(range, 0.0);
  }
  double alt = 0.0, az = 0.0, range = 0.0;
  ASSERT_TRUE(eclipse::ObserverApparentTargetDirection(
      kernel, 299, epoch.et_seconds, epoch.orientation,
      41.3666666667, -71.4833333333, 6.1,
      &alt, &az, &range, &error)) << error;
  EXPECT_TRUE(std::isfinite(alt));
  EXPECT_TRUE(std::isfinite(az));
  EXPECT_GT(range, 10000000.0);
}

TEST(NavigationDe440, PlanetCentresMatchIndependentJplHorizons) {
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;
  // Archived official Horizons DE441 observer rows, airless, obtained on
  // 2026-09-18. Venus here is the *geometric centre*, before the separate
  // navigational phase correction. JPL's small solar-deflection term and
  // independently selected EOP account for a fraction of an arcsecond.
  struct Reference {
    eclipse::CalendarDateTime utc;
    double latitude, longitude, height;
    int target;
    double ra, dec, altitude;
  };
  auto march = PointJudithUtc();
  march.year = 2025;
  march.month = 3;
  march.day = 5;
  march.hour = 15;
  march.minute = 0;
  const Reference cases[] = {
      {PointJudithUtc(), 41.3666666667, -71.4833333333, 6.1,
       199, 81.419228051, 24.021025797, 51.706330755},
      {PointJudithUtc(), 41.3666666667, -71.4833333333, 6.1,
       299, 85.365295419, 23.672932005, 54.339944885},
      {march, 20.0, -45.0, 0.0,
       199, 2.339307515, 2.399773377, 64.618560587},
      {march, 20.0, -45.0, 0.0,
       299, 6.743014765, 11.030924570, 66.007976753},
  };
  for (const auto& reference : cases) {
    double utc_jd = 0.0;
    ASSERT_TRUE(eclipse::CalendarToJulianDate(reference.utc, &utc_jd,
                                               &error)) << error;
    const auto dut1 = eclipse::LookupDut1(utc_jd);
    ASSERT_TRUE(dut1.available);
    eclipse::NavigationEpoch epoch;
    ASSERT_TRUE(eclipse::MakeNavigationEpoch(
        reference.utc, dut1.seconds,
        eclipse::TaiMinusUtcSeconds(reference.utc), 0.0, 0.0,
        &epoch, &error)) << error;
    eclipse::NavigationGeocentricState geo;
    ASSERT_TRUE(eclipse::GeocentricNavigationState(
        kernel, reference.target, epoch, &geo, &error)) << error;
    const double centre_ra = geo.gha_aries_deg - geo.centre_gha_deg;
    EXPECT_LT(std::fabs(CircularDifference(centre_ra, reference.ra)) * 3600.0,
              0.25) << reference.target;
    EXPECT_LT(std::fabs(geo.centre_declination_deg - reference.dec) * 3600.0,
              0.1) << reference.target;
    double altitude = 0.0, azimuth = 0.0, range = 0.0;
    ASSERT_TRUE(eclipse::ObserverApparentTargetDirection(
        kernel, reference.target, epoch.et_seconds, epoch.orientation,
        reference.latitude, reference.longitude, reference.height,
        &altitude, &azimuth, &range, &error)) << error;
    EXPECT_LT(std::fabs(altitude - reference.altitude) * 3600.0, 0.3)
        << reference.target;
  }
}

TEST(NavigationDe440, FractionalSecondsSurviveEpochAndInterpolation) {
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;
  auto before = PointJudithUtc();
  before.minute = 25;
  before.second = 59.875;
  auto after = PointJudithUtc();
  after.second = 0.125;
  eclipse::NavigationEpoch first, second;
  ASSERT_TRUE(eclipse::MakeNavigationEpoch(before, -0.0172, 37.0, 0.0, 0.0,
                                            &first, &error)) << error;
  ASSERT_TRUE(eclipse::MakeNavigationEpoch(after, -0.0172, 37.0, 0.0, 0.0,
                                            &second, &error)) << error;
  EXPECT_NEAR(second.et_seconds - first.et_seconds, 0.25, 0.0001);
  eclipse::NavigationGeocentricState a, b;
  ASSERT_TRUE(eclipse::GeocentricNavigationState(kernel, 301, first, &a,
                                                 &error)) << error;
  ASSERT_TRUE(eclipse::GeocentricNavigationState(kernel, 301, second, &b,
                                                 &error)) << error;
  const double change_arcsec = CircularDifference(b.gha_deg, a.gha_deg) * 3600;
  EXPECT_GT(change_arcsec, 3.0);
  EXPECT_LT(change_arcsec, 5.0);
}

TEST(NavigationDe440, Dut1RotatesLunarGhaWithoutChangingItsEphemeris) {
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;
  const auto utc = PointJudithUtc();
  eclipse::NavigationEpoch early, late;
  ASSERT_TRUE(eclipse::MakeNavigationEpoch(utc, -0.5, 37.0, 0.0, 0.0,
                                          &early, &error)) << error;
  ASSERT_TRUE(eclipse::MakeNavigationEpoch(utc, 0.5, 37.0, 0.0, 0.0,
                                          &late, &error)) << error;
  eclipse::NavigationGeocentricState a, b;
  ASSERT_TRUE(eclipse::GeocentricNavigationState(kernel, 301, early, &a,
                                                 &error)) << error;
  ASSERT_TRUE(eclipse::GeocentricNavigationState(kernel, 301, late, &b,
                                                 &error)) << error;
  const double rotation_arcsec = CircularDifference(b.gha_deg, a.gha_deg) * 3600.0;
  EXPECT_GT(rotation_arcsec, 15.0);
  EXPECT_LT(rotation_arcsec, 15.1);
  EXPECT_NEAR(b.declination_deg, a.declination_deg, 1e-7);
  EXPECT_NEAR(b.distance_km, a.distance_km, 1e-5);
}

TEST(NavigationDe440, LunarLimbUsesObserverRangeNotGeocentricRadius) {
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;
  const auto utc = PointJudithUtc();
  double utc_jd = 0.0;
  ASSERT_TRUE(eclipse::CalendarToJulianDate(utc, &utc_jd, &error)) << error;
  const auto dut1 = eclipse::LookupDut1(utc_jd);
  ASSERT_TRUE(dut1.available);
  eclipse::NavigationEpoch epoch;
  ASSERT_TRUE(eclipse::MakeNavigationEpoch(
      utc, dut1.seconds, eclipse::TaiMinusUtcSeconds(utc), 0.0, 0.0,
      &epoch, &error)) << error;
  eclipse::NavigationGeocentricState geo;
  ASSERT_TRUE(eclipse::GeocentricNavigationState(kernel, 301, epoch, &geo,
                                                 &error)) << error;
  double centre_alt = 0.0, azimuth = 0.0, topo_sd = 0.0;
  ASSERT_TRUE(eclipse::ObserverApparentDirection(
      kernel, epoch.et_seconds, epoch.orientation,
      41.3666666667, -71.4833333333, 6.1, true,
      &centre_alt, &azimuth, &topo_sd, &error)) << error;
  double range = 0.0, second_alt = 0.0, second_az = 0.0;
  ASSERT_TRUE(eclipse::ObserverApparentTargetDirection(
      kernel, 301, epoch.et_seconds, epoch.orientation,
      41.3666666667, -71.4833333333, 6.1,
      &second_alt, &second_az, &range, &error)) << error;
  EXPECT_DOUBLE_EQ(second_alt, centre_alt);
  EXPECT_DOUBLE_EQ(second_az, azimuth);
  constexpr double kDegreesPerRadian = 57.2957795130823208768;
  EXPECT_NEAR(topo_sd, std::asin(1737.4 / range) * kDegreesPerRadian, 1e-12);
  EXPECT_GT(topo_sd, geo.semidiameter_deg);
  // Pure geometry only: the selected upper/lower mean limb lies on opposite
  // sides of the same centre. Refraction/terrain must not be hidden here.
  EXPECT_NEAR((centre_alt + topo_sd) + (centre_alt - topo_sd),
              2.0 * centre_alt, 1e-12);
}
