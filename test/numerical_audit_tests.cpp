#include <gtest/gtest.h>

#include "AlmanacGenerator.h"
#include "NavigationAlgorithms.h"
#include "SextantCalibrationEngine.h"
#include "Sight.h"
#include "UtcDateTime.h"

#include <cmath>
#include <limits>
#include <sstream>

namespace {
constexpr double rad = 3.14159265358979323846 / 180.0;
wxDateTime Fields(const char* text) {
  wxDateTime value;
  EXPECT_TRUE(value.ParseISOCombined(text));
  return value;
}
double ReferenceBearing(double lat, double lon, double dec, double gpLon) {
  // USNO alt_az: LHA = observer east longitude - body's east GP longitude.
  const double h = (lon - gpLon) * rad;
  double az =
      std::atan2(-std::cos(dec * rad) * std::sin(h),
                 std::sin(dec * rad) * std::cos(lat * rad) -
                     std::cos(dec * rad) * std::sin(lat * rad) * std::cos(h)) /
      rad;
  return az < 0 ? az + 360 : az;
}
double PrintedAngle(const wxString& text) {
  double degrees = 0, minutes = 0;
  std::istringstream input(text.ToStdString());
  EXPECT_TRUE(static_cast<bool>(input >> degrees >> minutes));
  return degrees + minutes / 60;
}
std::vector<FixObservation> FixSights(const ObserverMotion& truth) {
  std::vector<FixObservation> sights;
  for (const char* body : {"Sun", "Moon", "Venus", "Arcturus"}) {
    FixObservation sight;
    sight.body = body;
    sight.utc = truth.referenceUtc;
    sight.observedAltitude =
        CelestialEphemeris::Evaluate(body, sight.utc, truth.latitude,
                                     truth.longitude)
            .geometricAltitude;
    sight.uncertaintyMinutes = 1;
    sights.push_back(sight);
  }
  return sights;
}

TEST(NumericalAudit, AzimuthHasCorrectQuadrantAcrossEquatorAndDateline) {
  Sight sight;
  for (double lat : {-60., -0.000001, 0., 0.000001, 60.})
    for (double lon : {-180., 0., 180.})
      for (double gp : {-60., 60., 179.}) {
        double hc, az;
        sight.AltitudeAzimuth(lat, lon, 20, gp, &hc, &az);
        EXPECT_NEAR(
            0, std::remainder(az - ReferenceBearing(lat, lon, 20, gp), 360),
            1e-8)
            << lat << "," << lon << "," << gp;
      }
}

TEST(NumericalAudit, CalibrationUsesObserverLongitudeNotItsReflection) {
  namespace sc = sextant_calibration;
  for (double sign : {-1., 1.}) {
    sc::BodySample first, second;
    first.geographic_longitude_deg = sign * 20;
    second.geographic_longitude_deg = sign * 40;
    sc::Environment environment;
    environment.observer = {0, sign * 20};
    environment.pressure_hpa = 0;
    const auto vacuum =
        sc::PredictApparentCenterDistance(first, second, environment);
    ASSERT_TRUE(vacuum.valid);
    EXPECT_NEAR(90, vacuum.first_altitude_deg, 1e-8);
    EXPECT_NEAR(70, vacuum.second_altitude_deg, 1e-8);
    EXPECT_NEAR(20, vacuum.apparent_center_distance_deg, 1e-8);
    first.horizontal_parallax_deg = 1;
    const auto moon =
        sc::PredictApparentCenterDistance(first, second, environment);
    ASSERT_TRUE(moon.valid);
    EXPECT_NEAR(20, moon.apparent_center_distance_deg, 1e-8);
    // For atmospheric separation avoid the undefined azimuth at exact zenith:
    // two bodies at 80/70 degrees on opposite sides of the meridian.
    first.horizontal_parallax_deg = 0;
    first.geographic_longitude_deg = sign * 10;
    environment.pressure_hpa = 1010;
    environment.temperature_c = 10;
    const auto air =
        sc::PredictApparentCenterDistance(first, second, environment);
    ASSERT_TRUE(air.valid);
    // Independent hand evaluation of the documented standard refraction model.
    EXPECT_NEAR(29.99089799536822, air.apparent_center_distance_deg, 1e-8);
  }
}

TEST(NumericalAudit, FindBodyUsesEffectiveTimeAndPreservesRecordedData) {
  for (double correction : {-120.5, 0., 120.5}) {
    Sight sight(Sight::ALTITUDE, "Sun", Sight::LOWER,
                Fields("2026-08-16T23:59:30"), 0, 20, 0.2);
    sight.m_DRLat = 35;
    sight.m_DRLon = -120;
    sight.Recompute(correction);
    const auto recorded = sight.m_DateTime;
    const auto ho = sight.m_ObservedAltitude;
    double hc, az;
    sight.CalculateAtDR(&hc, &az);
    const auto expected = CelestialEphemeris::Evaluate(
        "Sun",
        UtcDateTime::ToInstant(recorded) +
            wxTimeSpan::Milliseconds(static_cast<long long>(correction * 1000)),
        35, -120);
    EXPECT_NEAR(expected.geometricAltitude, hc, 1e-10);
    EXPECT_NEAR(expected.azimuthTrue, az, 1e-10);
    EXPECT_EQ(recorded, sight.m_DateTime);
    EXPECT_EQ(ho, sight.m_ObservedAltitude);
    EXPECT_EQ(20, sight.m_Measurement);
  }
}

TEST(NumericalAudit, SingularBearingIsExplicitWithoutLosingAltitude) {
  Sight sight;
  double hc, az;
  sight.AltitudeAzimuth(35, 20, 35, 20, &hc, &az);
  EXPECT_NEAR(90, hc, 1e-10);
  EXPECT_TRUE(std::isnan(az));
}

TEST(NumericalAudit, PlanetaryParallaxUsesKilometres) {
  const auto utc = UtcDateTime::ToInstant(Fields("2025-05-04T13:00:00"));
  for (const char* body : {"Mercury", "Venus", "Mars", "Jupiter", "Saturn"}) {
    const auto state =
        CelestialEphemeris::Evaluate(body, utc, -36.3352, 121.7511, 0);
    ASSERT_TRUE(state.valid);
    const double hp = std::asin(6378.14 / state.distance) / rad;
    EXPECT_NEAR(hp, state.horizontalParallax, 1e-10) << body;
    EXPECT_NEAR(
        state.geometricAltitude - hp * std::cos(state.geometricAltitude * rad),
        state.apparentAltitude, 1e-10);
  }
}

TEST(NumericalAudit, EstimatedHsReallyRoundTripsThroughReduction) {
  for (const char* body : {"Sun", "Moon", "Venus", "Sirius"})
    for (auto limb : {Sight::LOWER, Sight::CENTER, Sight::UPPER})
      for (int horizon = 0; horizon < 3; ++horizon)
        for (double altitude : {1., 5., 30., 70., 85.}) {
          Sight sight(Sight::ALTITUDE, body, limb,
                      Fields("2025-08-16T09:06:30"), 0, 20, 0.2);
          sight.m_EyeHeight = 3;
          sight.m_IndexError = -0.5;
          sight.m_Pressure = 1010;
          sight.m_Temperature = 10;
          sight.m_ArtificialHorizon = horizon == 1;
          sight.m_DipShort = horizon == 2;
          sight.m_DipShortDistance = 1;
          sight.Recompute(23.5);
          const double originalMeasurement = sight.m_Measurement;
          const double originalHo = sight.m_ObservedAltitude;
          double hs, error;
          sight.EstimateHs(altitude, &hs, &error);
          ASSERT_TRUE(std::isfinite(hs)) << body << "," << altitude;
          EXPECT_EQ(originalMeasurement, sight.m_Measurement);
          EXPECT_EQ(originalHo, sight.m_ObservedAltitude);
          Sight reduced = sight;
          reduced.m_Measurement = hs;
          reduced.Recompute(23.5);
          EXPECT_NEAR(altitude, reduced.m_ObservedAltitude, 1e-8)
              << body << "," << limb << "," << horizon << "," << altitude;
          EXPECT_NEAR((reduced.m_ObservedAltitude - altitude) * 60, error,
                      1e-8);
        }
}

TEST(NumericalAudit, RunningFixRetainsMeasurementUncertainty) {
  ObserverMotion truth;
  truth.referenceUtc = UtcDateTime::ToInstant(Fields("2027-06-01T12:00:00"));
  truth.latitude = 42.25;
  truth.longitude = -18.75;
  auto sights = FixSights(truth);
  const auto result = RunningFixSolver::Solve(sights, truth, 43, -17.5);
  ASSERT_TRUE(result.valid) << result.error;
  EXPECT_GT(result.semiMajorNm, 0.5);
  EXPECT_LT(result.semiMajorNm, 3);
  for (auto& sight : sights) sight.uncertaintyMinutes *= 2;
  const auto wider = RunningFixSolver::Solve(sights, truth, 43, -17.5);
  ASSERT_TRUE(wider.valid);
  EXPECT_NEAR(result.latitude, wider.latitude, 1e-8);
  EXPECT_NEAR(2 * result.semiMajorNm, wider.semiMajorNm, 1e-7);
}

TEST(NumericalAudit, RunningFixRejectsNonConvergenceAndInvalidData) {
  ObserverMotion truth;
  truth.referenceUtc = UtcDateTime::ToInstant(Fields("2027-06-01T12:00:00"));
  truth.latitude = 42.25;
  truth.longitude = -18.75;
  auto sights = FixSights(truth);
  EXPECT_FALSE(RunningFixSolver::Solve(sights, truth, 43, -17.5, 0).valid);
  EXPECT_FALSE(RunningFixSolver::Solve(sights, truth, 43, -17.5, 1).valid);
  EXPECT_FALSE(
      RunningFixSolver::Solve({sights[0], sights[0]}, truth, 43, -17.5).valid);
  sights[0].observedAltitude = NAN;
  EXPECT_FALSE(RunningFixSolver::Solve(sights, truth, 43, -17.5).valid);
  const auto utc = truth.referenceUtc;
  EXPECT_TRUE(std::isnan(SolveLatitudeFromAltitude("Sun", utc, 0, 91, 35)));
  EXPECT_TRUE(std::isnan(SolveLatitudeFromAltitude("Sun", utc, 0, NAN, 35)));
}

TEST(NumericalAudit,
     RunningFixNoisyUnequalWeightsHaveFiniteConservativeErrors) {
  ObserverMotion truth;
  truth.referenceUtc = UtcDateTime::ToInstant(Fields("2027-06-01T12:00:00"));
  truth.latitude = 42.25;
  truth.longitude = -18.75;
  auto sights = FixSights(truth);
  const auto exact = RunningFixSolver::Solve(sights, truth, 43, -17.5);
  for (size_t i = 0; i < sights.size(); ++i) {
    sights[i].observedAltitude += (i % 2 ? -2.0 : 2.0) / 60;
    sights[i].uncertaintyMinutes = 1 + 0.5 * i;
  }
  const auto noisy = RunningFixSolver::Solve(sights, truth, 43, -17.5);
  ASSERT_TRUE(noisy.valid) << noisy.error;
  EXPECT_TRUE(std::isfinite(noisy.semiMinorNm));
  EXPECT_GE(noisy.semiMajorNm, noisy.semiMinorNm);
  EXPECT_GT(noisy.semiMajorNm, exact.semiMajorNm);
}

TEST(NumericalAudit, AlmanacRowsAndCurvesUseLabelledUtcEpoch) {
  // CTest runs this fixture in separate UTC/BST/western/eastern processes.
  // Reference epoch is an actual instant, independently of document fields.
  for (const char* date :
       {"2026-01-16T00:00:00", "2026-08-16T00:00:00", "2026-03-29T00:00:00",
        "2026-03-08T00:00:00", "2026-10-25T00:00:00", "2026-11-01T00:00:00"}) {
    AlmanacRequest request;
    request.fromUtc = request.toUtc = Fields(date);
    request.latitude = 35;
    request.longitude = -20;
    const auto document = AlmanacGenerator::Build(request);
    bool foundGha = false, foundCurve = false;
    const auto midnight = UtcDateTime::ToInstant(Fields(date));
    for (const auto& page : document.pages) {
      if (page.section == "Daily ephemeris")
        for (const auto& table : page.tables)
          for (size_t col = 0; col < table.headings.size(); ++col)
            if (table.headings[col] == "Sun GHA") {
              ASSERT_EQ(24u, table.rows.size());
              for (size_t hour = 0; hour < 24; ++hour) {
                const auto state = CelestialEphemeris::Evaluate(
                    "Sun", midnight + wxTimeSpan::Hours(hour), 0, 0);
                EXPECT_NEAR(
                    0,
                    std::remainder(
                        PrintedAngle(table.rows[hour][col]) - state.gha, 360),
                    0.00084);
              }
              foundGha = true;
            }
      for (const auto& plot : page.plots)
        if (plot.title == "Apparent altitude through the UTC day")
          for (const auto& series : plot.series)
            for (size_t i = 0; i < series.x.size(); ++i) {
              const auto state = CelestialEphemeris::Evaluate(
                  series.label,
                  midnight + wxTimeSpan::Hours(static_cast<int>(series.x[i])),
                  request.latitude, request.longitude);
              EXPECT_NEAR(state.apparentAltitude, series.y[i], 1e-8);
              foundCurve = true;
            }
    }
    EXPECT_TRUE(foundGha);
    EXPECT_TRUE(foundCurve);
  }
}
}  // namespace
