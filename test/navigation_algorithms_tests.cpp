#include <gtest/gtest.h>

#include "BodyCatalog.h"
#include "NavigationUIUtils.h"
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"
#include "geodesic.h"

namespace {
wxDateTime UtcFields(const char* text) {
  wxDateTime value;
  EXPECT_TRUE(value.ParseISOCombined(text));
  return value;
}

wxDateTime Utc(const char* text) {
  return UtcDateTime::ToInstant(UtcFields(text));
}

FixObservation Synthetic(const wxString& body, const wxDateTime& utc,
                         const ObserverMotion& truth, double noiseMinutes = 0) {
  double lat, lon;
  truth.PositionAt(utc, &lat, &lon);
  const BodyState state = CelestialEphemeris::Evaluate(body, utc, lat, lon);
  EXPECT_TRUE(state.valid);
  FixObservation observation;
  observation.label = body;
  observation.body = body;
  observation.utc = utc;
  observation.observedAltitude =
      state.geometricAltitude + noiseMinutes / 60.0;
  observation.uncertaintyMinutes = 1.0;
  return observation;
}

int UtcMinute(const wxDateTime& utc) {
  long hour = 0, minute = 0;
  utc.Format("%H", wxDateTime::UTC).ToLong(&hour);
  utc.Format("%M", wxDateTime::UTC).ToLong(&minute);
  return static_cast<int>(hour * 60 + minute);
}

int EventMinute(const DailyEventsResult& events, HorizonEventKind kind) {
  for (const auto& event : events.events) {
    if (event.kind == kind) return UtcMinute(event.utc);
  }
  return -1;
}
}  // namespace

TEST(UtcDateTime, FormatsPluginUtcFieldsWithoutApplyingLocalOffsetTwice) {
  const wxDateTime utc = UtcFields("2026-08-13T04:48:12");
  EXPECT_EQ("2026-08-13 04:48:12",
            UtcDateTime::FormatUtc(utc, "%Y-%m-%d %H:%M:%S"));
  EXPECT_EQ("2026-08-13 05:48:12",
            UtcDateTime::FormatOffset(utc, 3600,
                                      "%Y-%m-%d %H:%M:%S"));
  EXPECT_EQ("2026-08-13T04:48:12Z", UtcDateTime::FormatIsoUtc(utc));
}

TEST(UtcDateTime, LegacyUtcFieldsRoundTripInWinterAndSummer) {
  for (const char* text : {"2026-01-13T12:34:56",
                           "2026-08-13T12:34:56"}) {
    const wxDateTime utc = UtcFields(text);
    const wxDateTime roundTrip =
        UtcDateTime::FromLocalFields(UtcDateTime::ToLocalFields(utc));
    EXPECT_EQ(UtcDateTime::FormatIsoUtc(utc),
              UtcDateTime::FormatIsoUtc(roundTrip));
  }
}

TEST(UtcDateTime, CurrentUtcFieldsRepresentTheCurrentInstant) {
  const wxDateTime before = wxDateTime::UNow();
  const wxDateTime utcFields = UtcDateTime::Now();
  const wxDateTime after = wxDateTime::UNow();
  const wxDateTime instant = UtcDateTime::ToInstant(utcFields);
  EXPECT_FALSE(instant.IsEarlierThan(before - wxTimeSpan::Seconds(1)));
  EXPECT_FALSE(instant.IsLaterThan(after + wxTimeSpan::Seconds(1)));
}

TEST(NavigationAngles, AcceptsDecimalDegreesMinutesAndSeconds) {
  double value = 0.0;
  EXPECT_TRUE(ParseNavigationAngle("43.236698 N", NavigationAngleKind::Latitude,
                                   -90.0, 90.0, &value));
  EXPECT_NEAR(43.236698, value, 1e-6);
  EXPECT_TRUE(ParseNavigationAngle("-58.4699", NavigationAngleKind::Generic,
                                   -180.0, 180.0, &value));
  EXPECT_NEAR(-58.4699, value, 1e-6);
  EXPECT_TRUE(ParseNavigationAngle(
      "43 14.2019 N", NavigationAngleKind::Latitude, -90.0, 90.0, &value));
  EXPECT_NEAR(43.2366983, value, 1e-6);
  EXPECT_TRUE(ParseNavigationAngle("-58 28.2", NavigationAngleKind::Generic,
                                   -180.0, 180.0, &value));
  EXPECT_NEAR(-58.47, value, 1e-6);
  EXPECT_TRUE(ParseNavigationAngle("077 31 18.114 W",
                                   NavigationAngleKind::Longitude, -180.0,
                                   180.0, &value));
  EXPECT_NEAR(-77.5216983, value, 1e-6);
  EXPECT_FALSE(ParseNavigationAngle("91 N", NavigationAngleKind::Latitude,
                                    -90.0, 90.0, &value));
}

TEST(BodyCatalog, UsesUniqueNamesUnderstoodByEphemeris) {
  ASSERT_GT(BodyCatalog::All().size(), 60u);
  for (const auto& body : BodyCatalog::All()) {
    EXPECT_EQ(&body, BodyCatalog::Find(body.name));
    const BodyState state = CelestialEphemeris::Evaluate(
        body.name, Utc("2027-08-02T10:00:00"), 36.0, -5.0);
    EXPECT_TRUE(state.valid) << body.name;
  }
}

TEST(Ephemeris, ProducesPlausible2027SolarCoordinates) {
  const BodyState sun = CelestialEphemeris::Evaluate(
      "Sun", Utc("2027-08-02T10:00:00"), 36.0, -5.0);
  ASSERT_TRUE(sun.valid);
  EXPECT_GT(sun.declination, 16.0);
  EXPECT_LT(sun.declination, 18.0);
  EXPECT_GT(sun.geometricAltitude, 45.0);
  EXPECT_LT(sun.geometricAltitude, 75.0);
  EXPECT_NEAR(0.266, sun.semidiameter, 0.01);
}

TEST(Ephemeris, OrdinaryPlannerCoversDocumented1900To2100Range) {
  for (const char* text : {"1900-01-01T00:00:00", "2100-12-31T23:00:00"}) {
    EXPECT_TRUE(CelestialEphemeris::Evaluate("Sun", Utc(text), 0, 0).valid);
    EXPECT_TRUE(CelestialEphemeris::Evaluate("Moon", Utc(text), 0, 0).valid);
    EXPECT_TRUE(CelestialEphemeris::Evaluate("Polaris", Utc(text), 50, 0).valid);
  }
}

TEST(MoonPlanning, ReturnsTheNextFourPrincipalPhasesChronologically) {
  const wxDateTime start = Utc("2027-08-02T00:00:00");
  const auto phases = NextPrincipalMoonPhases(start, 36.0, -5.0);
  ASSERT_EQ(4u, phases.size());
  for (size_t i = 0; i < phases.size(); ++i) {
    EXPECT_TRUE(phases[i].utc.IsLaterThan(start));
    EXPECT_LT((phases[i].utc - start).GetDays(), 31);
    if (i) {
      EXPECT_TRUE(phases[i].utc.IsLaterThan(phases[i - 1].utc));
    }
  }
}

TEST(HorizonEvents, GivesChronologicalOfflineTwilightAndRiseSetTable) {
  ObserverMotion observer;
  observer.referenceUtc = Utc("2026-08-13T00:00:00");
  observer.latitude = 54.6;
  observer.longitude = -5.9;
  const DailyEventsResult events = HorizonEventCalculator::Calculate(
      observer.referenceUtc, observer, 2.0);
  ASSERT_GE(events.events.size(), 10u);
  for (size_t i = 1; i < events.events.size(); ++i)
    EXPECT_FALSE(events.events[i].utc.IsEarlierThan(events.events[i - 1].utc));
  bool sunrise = false, sunset = false, noon = false;
  for (const auto& event : events.events) {
    sunrise |= event.kind == HorizonEventKind::Sunrise;
    sunset |= event.kind == HorizonEventKind::Sunset;
    noon |= event.kind == HorizonEventKind::UpperTransit;
  }
  EXPECT_TRUE(sunrise);
  EXPECT_TRUE(sunset);
  EXPECT_TRUE(noon);
}

TEST(HorizonEvents, ReportsPolarDayWithoutInventingSunrise) {
  ObserverMotion observer;
  observer.referenceUtc = Utc("2027-06-21T00:00:00");
  observer.latitude = 80.0;
  observer.longitude = 0.0;
  const auto events =
      HorizonEventCalculator::Calculate(observer.referenceUtc, observer);
  EXPECT_TRUE(events.sunAlwaysAbove);
  for (const auto& event : events.events)
    EXPECT_NE(HorizonEventKind::Sunrise, event.kind);
}

TEST(HorizonEvents, GreenwichSolsticeMatchesPublishedCivilTimes) {
  ObserverMotion observer;
  observer.referenceUtc = Utc("2024-06-21T00:00:00");
  observer.latitude = 51.4779;
  observer.longitude = 0.0;
  const auto events =
      HorizonEventCalculator::Calculate(observer.referenceUtc, observer, 0.0)
          .events;
  int sunriseMinutes = -1, sunsetMinutes = -1;
  for (const auto& event : events) {
    if (event.kind == HorizonEventKind::Sunrise)
      sunriseMinutes = UtcMinute(event.utc);
    if (event.kind == HorizonEventKind::Sunset)
      sunsetMinutes = UtcMinute(event.utc);
  }
  // Published Greenwich civil times are about 03:43 and 20:21 UTC.  A
  // several-minute tolerance acknowledges the real horizon/refraction model.
  EXPECT_NEAR(3 * 60 + 43, sunriseMinutes, 5);
  EXPECT_NEAR(20 * 60 + 21, sunsetMinutes, 5);
}

TEST(HorizonEvents, PublishedUtcTimesAcrossLongitudesAndHemispheres) {
  struct Location {
    const char* name;
    double latitude;
    double longitude;
    int sunriseUtcMinute;
    int sunsetUtcMinute;
  };
  // Approximate published civil rise/set times for 2026-08-13.  Tolerance
  // covers rounding and small differences in horizon/refraction conventions.
  const Location locations[] = {
      {"Chester", 53.1833, -2.8767, 4 * 60 + 48, 19 * 60 + 43},
      {"New York", 40.7128, -74.0060, 10 * 60 + 4, 23 * 60 + 56},
      {"Quito", -0.1807, -78.4678, 11 * 60 + 15, 23 * 60 + 22},
      {"Cape Town", -33.9249, 18.4241, 5 * 60 + 27, 16 * 60 + 15},
      // A UTC day in Sydney contains the local sunset followed by the next
      // local sunrise, which is the expected ordering for this UTC API.
      {"Sydney", -33.8688, 151.2093, 20 * 60 + 35, 7 * 60 + 23},
  };
  for (const auto& location : locations) {
    ObserverMotion observer;
    observer.referenceUtc = Utc("2026-08-13T00:00:00");
    observer.latitude = location.latitude;
    observer.longitude = location.longitude;
    const DailyEventsResult events = HorizonEventCalculator::Calculate(
        observer.referenceUtc, observer, 0.0);
    EXPECT_NEAR(location.sunriseUtcMinute,
                EventMinute(events, HorizonEventKind::Sunrise), 6)
        << location.name;
    EXPECT_NEAR(location.sunsetUtcMinute,
                EventMinute(events, HorizonEventKind::Sunset), 6)
        << location.name;
  }
}

TEST(MotionModel, PropagatesPositionAtBoatSpeed) {
  ObserverMotion observer;
  observer.referenceUtc = Utc("2027-06-01T12:00:00");
  observer.latitude = 50.0;
  observer.longitude = -20.0;
  observer.courseTrue = 90.0;
  observer.speedKnots = 12.0;
  observer.moving = true;
  double lat, lon;
  observer.PositionAt(observer.referenceUtc + wxTimeSpan::Hours(2), &lat, &lon);
  double bearing, distance;
  ll_gc_ll_reverse(observer.latitude, observer.longitude, lat, lon, &bearing,
                   &distance);
  EXPECT_NEAR(24.0, distance, 0.05);
  EXPECT_NEAR(90.0, bearing, 0.2);
}

TEST(MotionModel, PropagationUsesElapsedUtcAcrossDstTransitionDates) {
  ObserverMotion observer;
  observer.referenceUtc = Utc("2026-03-29T00:30:00");
  observer.latitude = 50.0;
  observer.longitude = -5.0;
  observer.courseTrue = 90.0;
  observer.speedKnots = 10.0;
  observer.moving = true;
  double lat, lon, bearing, distance;
  observer.PositionAt(Utc("2026-03-29T02:30:00"), &lat, &lon);
  ll_gc_ll_reverse(observer.latitude, observer.longitude, lat, lon, &bearing,
                   &distance);
  EXPECT_NEAR(20.0, distance, 0.05);
}

TEST(SightRanking, PrefersNearOrthogonalPairGeometry) {
  RankedBody north, east, nearby;
  north.state.body = "North";
  north.state.azimuthTrue = 0.0;
  north.score = 80.0;
  east.state.body = "East";
  east.state.azimuthTrue = 90.0;
  east.score = 80.0;
  nearby.state.body = "Nearby";
  nearby.state.azimuthTrue = 12.0;
  nearby.score = 80.0;
  const auto pairs = SightRanker::BestCombinations({north, nearby, east}, 2, 3);
  ASSERT_FALSE(pairs.empty());
  EXPECT_EQ("North", pairs.front().bodies[0].state.body);
  EXPECT_EQ("East", pairs.front().bodies[1].state.body);
}

TEST(RunningFix, RecoversACommonEpochPositionFromTimeTaggedSights) {
  ObserverMotion truth;
  truth.referenceUtc = Utc("2027-06-01T12:00:00");
  truth.latitude = 42.25;
  truth.longitude = -18.75;
  truth.courseTrue = 72.0;
  truth.speedKnots = 8.0;
  truth.moving = true;
  std::vector<FixObservation> observations = {
      Synthetic("Sun", truth.referenceUtc - wxTimeSpan::Minutes(35), truth),
      Synthetic("Moon", truth.referenceUtc - wxTimeSpan::Minutes(5), truth),
      Synthetic("Venus", truth.referenceUtc + wxTimeSpan::Minutes(25), truth),
      Synthetic("Arcturus", truth.referenceUtc + wxTimeSpan::Minutes(45), truth)};
  const RunningFixResult fix =
      RunningFixSolver::Solve(observations, truth, 43.0, -17.5);
  ASSERT_TRUE(fix.valid) << fix.error;
  EXPECT_NEAR(truth.latitude, fix.latitude, 0.01);
  EXPECT_NEAR(truth.longitude, fix.longitude, 0.01);
  EXPECT_LT(fix.rmsMinutes, 0.05);
}

TEST(SequenceAnalyzer, FindsBiasTrendAndGrossOutlier) {
  ObserverMotion truth;
  truth.referenceUtc = Utc("2027-06-01T12:00:00");
  truth.latitude = 42.25;
  truth.longitude = -18.75;
  std::vector<FixObservation> observations;
  for (int i = 0; i < 6; ++i) {
    const wxDateTime time = truth.referenceUtc + wxTimeSpan::Minutes(i * 10);
    observations.push_back(Synthetic("Sun", time, truth, 1.5 + i * 0.2));
  }
  observations[3].observedAltitude += 12.0 / 60.0;
  const SequenceStatistics analysis = SightSequenceAnalyzer::Analyze(
      observations, truth, truth.latitude, truth.longitude);
  ASSERT_TRUE(analysis.valid);
  EXPECT_NEAR(1.9, analysis.personalBiasMinutes, 0.5);
  EXPECT_NEAR(1.2, analysis.trendMinutesPerHour, 0.5);
  EXPECT_TRUE(analysis.residuals[3].outlier);
}

TEST(SequenceAnalyzer, MatchesBobBossertHistoricalSunSightHc) {
  ObserverMotion track;
  track.referenceUtc = Utc("2024-11-13T20:24:27");
  track.latitude = 43.2366983;
  track.longitude = -77.5216983;
  FixObservation observation;
  observation.label = "Sun";
  observation.body = "Sun";
  observation.utc = track.referenceUtc;
  observation.observedAltitude = 11.95556;
  const SequenceStatistics analysis = SightSequenceAnalyzer::Analyze(
      {observation, observation}, track, track.latitude, track.longitude);
  ASSERT_TRUE(analysis.valid);
  ASSERT_EQ(2u, analysis.residuals.size());
  EXPECT_NEAR(11.92667, analysis.residuals.front().calculatedAltitude, 0.01);
  EXPECT_NEAR(1.73, analysis.residuals.front().interceptMinutes, 0.7);
}

TEST(Almanac, ExportsAStableOfflineCsvTable) {
  ObserverMotion observer;
  observer.referenceUtc = Utc("2027-01-01T00:00:00");
  observer.latitude = 0.0;
  observer.longitude = 0.0;
  const auto rows = BuildAlmanac(observer.referenceUtc, 2,
                                 {"Sun", "Moon", "Polaris"}, observer);
  ASSERT_EQ(9u, rows.size());
  const wxString csv = AlmanacToCsv(rows);
  EXPECT_TRUE(csv.StartsWith("UTC,Body,GHA_deg"));
  EXPECT_NE(wxNOT_FOUND, csv.Find("Polaris"));
  EXPECT_NE(wxNOT_FOUND, csv.Find("2027-01-01T00:00:00Z,Sun"));
  EXPECT_EQ(wxNOT_FOUND, csv.Find("2027-01-01Z00:00:00"));
}

TEST(Almanac, HourlyUtcRowsDoNotSkipAtComputerDstBoundary) {
  ObserverMotion observer;
  observer.referenceUtc = Utc("2026-03-29T00:00:00");
  const auto rows =
      BuildAlmanac(observer.referenceUtc, 3, {"Sun"}, observer);
  ASSERT_EQ(4u, rows.size());
  EXPECT_EQ("2026-03-29T00:00:00Z",
            rows[0].utc.Format("%Y-%m-%dT%H:%M:%SZ", wxDateTime::UTC));
  EXPECT_EQ("2026-03-29T01:00:00Z",
            rows[1].utc.Format("%Y-%m-%dT%H:%M:%SZ", wxDateTime::UTC));
  EXPECT_EQ("2026-03-29T02:00:00Z",
            rows[2].utc.Format("%Y-%m-%dT%H:%M:%SZ", wxDateTime::UTC));
  EXPECT_EQ("2026-03-29T03:00:00Z",
            rows[3].utc.Format("%Y-%m-%dT%H:%M:%SZ", wxDateTime::UTC));
}

TEST(SpecialWorkflows, PolarisAltitudeRecoversLatitude) {
  const wxDateTime utc = Utc("2027-09-01T22:00:00");
  const double truth = 53.5, longitude = -6.0;
  const double altitude =
      CelestialEphemeris::Evaluate("Polaris", utc, truth, longitude)
          .geometricAltitude;
  const double solved =
      SolveLatitudeFromAltitude("Polaris", utc, longitude, altitude, 52.0);
  EXPECT_NEAR(truth, solved, 0.001);
}

TEST(SpecialWorkflows, NoonAltitudeRecoversLatitudeNearTheDrEstimate) {
  ObserverMotion observer;
  observer.referenceUtc = Utc("2026-08-13T00:00:00");
  observer.latitude = 35.0;
  observer.longitude = -10.0;
  const auto events =
      HorizonEventCalculator::Calculate(observer.referenceUtc, observer);
  wxDateTime transit;
  for (const auto& event : events.events)
    if (event.kind == HorizonEventKind::UpperTransit) transit = event.utc;
  ASSERT_TRUE(transit.IsValid());
  const double altitude =
      CelestialEphemeris::Evaluate("Sun", transit, observer.latitude,
                                   observer.longitude)
          .geometricAltitude;
  const double solved = SolveLatitudeFromAltitude(
      "Sun", transit, observer.longitude, altitude, 34.0);
  EXPECT_NEAR(observer.latitude, solved, 0.001);
}
