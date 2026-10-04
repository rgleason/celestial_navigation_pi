#include <gtest/gtest.h>

#include "NavigationEphemerisProvider.h"
#include "NavigationAlgorithms.h"
#include "Sight.h"
#include "UtcDateTime.h"
#include "astrolabe/astrolabe.hpp"
#include "eclipse/dut1.h"

#include <cmath>
#include <iostream>
#include <wx/utils.h>

namespace {
using celestial_navigation::AnalyticalNavigationEpoch;
using celestial_navigation::ResolveAnalyticalNavigationEpoch;
struct AnalyticalScope {
  wxString previous;
  bool hadPrevious = wxGetEnv("CELNAV_TEST_DE440_ENABLE", &previous);
  AnalyticalScope() { wxUnsetEnv("CELNAV_TEST_DE440_ENABLE"); }
  ~AnalyticalScope() {
    if (hadPrevious) wxSetEnv("CELNAV_TEST_DE440_ENABLE", previous);
    else wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
  }
};
double Difference(double a, double b) { return std::remainder(a - b, 360.0); }
}

TEST(AnalyticalEpoch, DatedDut1AndModernTtRemainSeparate) {
  const wxDateTime utc(11, wxDateTime::Mar, 2003, 12, 0, 0, 125);
  AnalyticalNavigationEpoch automatic, zero, override;
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(utc, &automatic));
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(utc, &zero, 0.0));
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(utc, &override, 0.456789));
  const auto dated = eclipse::LookupDut1(automatic.utc_jd);
  ASSERT_TRUE(dated.available);
  EXPECT_TRUE(automatic.modern_utc);
  EXPECT_TRUE(automatic.dut1_available);
  EXPECT_DOUBLE_EQ(automatic.dut1_seconds, dated.seconds);
  EXPECT_NEAR((automatic.ut1_jd - automatic.utc_jd) * 86400.0,
              dated.seconds, 0.00005);
  // In March 2003 TAI-UTC was 32 seconds, so TT-UTC was 64.184 s.
  EXPECT_NEAR((automatic.tt_jd - automatic.utc_jd) * 86400.0, 64.184, 0.00005);
  EXPECT_DOUBLE_EQ(zero.utc_jd, zero.ut1_jd);
  EXPECT_DOUBLE_EQ(override.dut1_seconds, 0.456789);
  EXPECT_DOUBLE_EQ(automatic.tt_jd, zero.tt_jd);
  EXPECT_DOUBLE_EQ(automatic.tt_jd, override.tt_jd);
}

TEST(AnalyticalEpoch, MoonEphemerisDoesNotMoveWithDut1Override) {
  AnalyticalScope scope;
  const wxDateTime instant = UtcDateTime::ToInstant(
      wxDateTime(13, wxDateTime::Jun, 2024, 19, 26, 0, 125));
  const auto a = CelestialEphemeris::Evaluate("Moon", instant, 0, 0, 0, 10, 0.0);
  const auto b = CelestialEphemeris::Evaluate("Moon", instant, 0, 0, 0, 10, 0.4);
  ASSERT_TRUE(a.valid && b.valid);
  EXPECT_FALSE(a.usedDe440);
  EXPECT_DOUBLE_EQ(a.declination, b.declination);
  EXPECT_DOUBLE_EQ(a.distance, b.distance);
  EXPECT_NEAR(Difference(b.gha, a.gha) * 3600.0, 0.4 * 15.041067, 0.001);
}

TEST(AnalyticalEpoch, OrdinarySightPlannerAndExplicitTableValueAgree) {
  AnalyticalScope scope;
  const wxDateTime fields(19, wxDateTime::Jan, 2006, 23, 0, 0, 375);
  AnalyticalNavigationEpoch epoch;
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(fields, &epoch));
  const wxDateTime instant = UtcDateTime::ToInstant(fields);
  for (const wxString body : {"Sun", "Moon", "Venus", "Mars", "Polaris"}) {
    Sight sight(Sight::ALTITUDE, body, Sight::CENTER, fields, 20, 0, 1);
    double declination = 0, longitude = 0;
    sight.BodyLocation(fields, &declination, &longitude, nullptr, nullptr, nullptr);
    const auto automatic = CelestialEphemeris::Evaluate(body, instant, 0, 0);
    const auto explicitDated = CelestialEphemeris::Evaluate(
        body, instant, 0, 0, 1010, 10, epoch.dut1_seconds);
    ASSERT_TRUE(automatic.valid && explicitDated.valid) << body;
    EXPECT_FALSE(automatic.usedDe440);
    EXPECT_DOUBLE_EQ(automatic.declination, declination);
    EXPECT_NEAR(Difference(automatic.gha, -longitude), 0, 1e-10);
    EXPECT_DOUBLE_EQ(automatic.gha, explicitDated.gha);
    EXPECT_DOUBLE_EQ(automatic.declination, explicitDated.declination);
    const auto zero = CelestialEphemeris::Evaluate(body, instant, 0, 0, 1010, 10, 0);
    EXPECT_NEAR(Difference(automatic.gha, zero.gha) * 3600.0,
                epoch.dut1_seconds * 15.041067, 0.001);
  }
}

TEST(AnalyticalEpoch, KernelInstalledStillCorrectsAnalyticalOnlyBodies) {
  AnalyticalScope scope;
  const wxDateTime instant = UtcDateTime::ToInstant(
      wxDateTime(19, wxDateTime::Jan, 2006, 23, 0, 0));
  for (const wxString body : {"Polaris", "Mars", "Jupiter", "Saturn"}) {
    wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
    const auto without = CelestialEphemeris::Evaluate(body, instant, 0, 0);
    wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
    const auto with = CelestialEphemeris::Evaluate(body, instant, 0, 0);
    ASSERT_TRUE(with.valid && without.valid) << body;
    EXPECT_FALSE(with.usedDe440);
    EXPECT_DOUBLE_EQ(with.gha, without.gha);
    EXPECT_DOUBLE_EQ(with.declination, without.declination);
  }
}

TEST(AnalyticalEpoch, PreviouslyMisalignedUtcCasesMatchArchivedUsno) {
  AnalyticalScope scope;
  // USNO Celnav API 4.0.1, archived during the 27 September investigation.
  // API arguments are UT1. Adjacent whole-second results were interpolated
  // to the dated DUT1 epoch, NOT compared at the same UTC clock reading.
  // https://aa.usno.navy.mil/api/celnav?date=2003-03-11&time=11%3A59%3A59&coords=-3.765%2C2.5316666666666947&ID=pob220
  // https://aa.usno.navy.mil/api/celnav?date=2006-01-19&time=23%3A00%3A00&coords=-20.208333333333332%2C-162.285&ID=pob220
  // https://aa.usno.navy.mil/api/celnav?date=2025-11-02&time=02%3A00%3A00&coords=-7.923333333333333%2C131.08166666666665&ID=pob220
  struct Row { const char* body; wxDateTime utc; double gha; double dec; };
  for (const auto& row : {
      Row{"Sun", wxDateTime(11, wxDateTime::Mar, 2003, 12, 0, 0),
          357.46483322424, -3.7649636364},
      Row{"Sun", wxDateTime(19, wxDateTime::Jan, 2006, 23, 0, 0),
          162.288744133225, -20.207744002305},
      Row{"Venus", wxDateTime(2, wxDateTime::Nov, 2025, 2, 0, 0),
          228.919174625655, -7.923001460535}}) {
    // Construct a real UTC instant at a safe daytime anchor. The legacy
    // field-to-instant helper itself is ambiguous at local DST transitions.
    const wxDateTime noon(row.utc.GetDay(), row.utc.GetMonth(), row.utc.GetYear(), 12);
    const wxDateTime instant = UtcDateTime::ToInstant(noon) +
        wxTimeSpan::Seconds((row.utc.GetHour() - 12) * 3600);
    const auto result = CelestialEphemeris::Evaluate(row.body, instant, 0, 0);
    ASSERT_TRUE(result.valid);
    const double ghaError = Difference(result.gha, row.gha) * 3600.0;
    const double decError = (result.declination - row.dec) * 3600.0;
    std::cout << row.body << " " << row.utc.FormatISOCombined('T')
              << " analytical-USNO: GHA " << ghaError << " arcsec, Dec "
              << decError << " arcsec\n";
    EXPECT_NEAR(ghaError, 0, 1.0);
    EXPECT_NEAR(decError, 0, 1.0);
  }
}

TEST(AnalyticalEpoch, FractionalSecondsAndLeapBoundaryArePreserved) {
  AnalyticalNavigationEpoch a, b, before, after;
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(
      wxDateTime(13, wxDateTime::Jun, 2024, 19, 26, 0, 125), &a));
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(
      wxDateTime(13, wxDateTime::Jun, 2024, 19, 26, 0, 375), &b));
  EXPECT_NEAR((b.tt_jd - a.tt_jd) * 86400.0, 0.25, 0.00005);
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(
      wxDateTime(30, wxDateTime::Jun, 2015, 23, 59, 59), &before));
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(
      wxDateTime(1, wxDateTime::Jul, 2015, 0, 0, 0), &after));
  // The intervening leap second is not an input wxDateTime can represent.
  EXPECT_NEAR((after.tt_jd - before.tt_jd) * 86400.0, 2.0, 0.00005);
  EXPECT_NEAR((after.ut1_jd - before.ut1_jd) * 86400.0, 2.0, 0.0001);
}

TEST(AnalyticalEpoch, RealInstantAvoidsLocalDstFieldReconstruction) {
  // March 8 02:30 does not exist as a local time in New York in 2026,
  // but it is a perfectly valid UTC instant. November exercises DST end.
  for (const auto date : {wxDateTime(8, wxDateTime::Mar, 2026, 12),
                          wxDateTime(2, wxDateTime::Nov, 2025, 12)}) {
    const wxDateTime instant = UtcDateTime::ToInstant(date) -
        wxTimeSpan::Hours(10) + wxTimeSpan::Minutes(30);
    AnalyticalNavigationEpoch epoch;
    ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(instant, &epoch, NAN, true));
    EXPECT_NEAR(epoch.utc_jd, instant.GetJulianDayNumber(), 1e-9);
    const auto dated = eclipse::LookupDut1(instant.GetJulianDayNumber());
    EXPECT_DOUBLE_EQ(epoch.dut1_seconds, dated.seconds);
  }
}

TEST(AnalyticalEpoch, NoCoverageFallsBackWithoutExpiryOrExtrapolation) {
  AnalyticalScope scope;
  AnalyticalNavigationEpoch epoch;
  const wxDateTime fields(1, wxDateTime::Jan, 2200, 0, 0, 0);
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(fields, &epoch));
  EXPECT_TRUE(epoch.modern_utc);
  EXPECT_FALSE(epoch.dut1_available);
  EXPECT_DOUBLE_EQ(epoch.dut1_seconds, 0);
  EXPECT_DOUBLE_EQ(epoch.utc_jd, epoch.ut1_jd);
  EXPECT_TRUE(std::isfinite(epoch.tt_jd));
  const auto moon = CelestialEphemeris::Evaluate(
      "Moon", UtcDateTime::ToInstant(fields), 0, 0);
  EXPECT_TRUE(moon.valid);
  EXPECT_FALSE(moon.usedDe440);
}

TEST(AnalyticalEpoch, HistoricalDeltaTIsRetainedAndInvalidInputIsAtomic) {
  AnalyticalNavigationEpoch epoch;
  ASSERT_TRUE(ResolveAnalyticalNavigationEpoch(
      wxDateTime(1, wxDateTime::Jan, 1900, 0, 0, 0), &epoch));
  EXPECT_FALSE(epoch.modern_utc);
  EXPECT_DOUBLE_EQ(epoch.ut1_jd, epoch.utc_jd);
  EXPECT_DOUBLE_EQ(epoch.tt_jd, astrolabe::dynamical::ut_to_dt(epoch.utc_jd));
  const auto original = epoch;
  EXPECT_FALSE(ResolveAnalyticalNavigationEpoch(wxDateTime(), &epoch));
  EXPECT_DOUBLE_EQ(epoch.tt_jd, original.tt_jd);
  EXPECT_FALSE(ResolveAnalyticalNavigationEpoch(wxDateTime::Now(), nullptr));
}

TEST(AnalyticalEpoch, CalculationTrailReportsTableAndUnavailableFallback) {
  AnalyticalScope scope;
  for (const int year : {2024, 2200}) {
    Sight sight(Sight::ALTITUDE, "Sun", Sight::CENTER,
                wxDateTime(13, wxDateTime::Jun, year, 19, 26, 0), 30, 0, 1);
    sight.Recompute(0);
    EXPECT_NE(sight.m_CalcStr.Find("Ephemeris = Classic analytical"), wxNOT_FOUND);
    EXPECT_NE(sight.m_CalcStr.Find(year == 2024 ? "bundled offline table"
        : "UT1=UTC fallback; table unavailable"), wxNOT_FOUND);
  }
}
