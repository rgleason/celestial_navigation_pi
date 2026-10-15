#include <gtest/gtest.h>
#include "LunarPresentation.h"
#include "LunarSolutionRecord.h"
#include "NavigationUIUtils.h"
#include "Sight.h"
#include "UtcDateTime.h"

namespace {
Sight Example() {
  const auto epoch = UtcDateTime::Create(2026, 9, 19, 1);
  Sight sight(Sight::LUNAR, "Saturn", Sight::LUNAR_FAR, epoch, 54800,
              104 + 34.0 / 60, .2, true);
  sight.m_AllowDe440 = false;
  sight.m_AllowCompact = false;
  sight.m_LunarMoonAltitude = 13 + 8.0 / 60;
  sight.m_LunarBodyAltitude = 11 + 33.0 / 60;
  sight.m_LunarMoonLimb = Sight::LOWER;
  sight.m_LunarBodyLimb = Sight::CENTER;
  sight.m_LunarMoonAltitudeUncertainty = .5;
  sight.m_LunarBodyAltitudeUncertainty = .5;
  sight.m_IndexError = -1.5;
  sight.m_EyeHeight = 3;
  sight.m_Temperature = 11;
  sight.m_Pressure = 1010;
  sight.m_DRLat = 43 + 16.7898 / 60;
  sight.m_DRLon = -(76 + 58.4122 / 60);
  sight.m_CorrectedDateTime = epoch;
  sight.RecomputeLunar();
  return sight;
}
const lunar_presentation::Table* Find(
    const std::vector<lunar_presentation::Table>& tables,
    const wxString& title) {
  for (const auto& table : tables)
    if (table.title == title) return &table;
  return nullptr;
}
}  // namespace

TEST(LunarPresentation, ResultUtcRoundsBeforeFormattingAcrossMidnight) {
  EXPECT_EQ("2027-01-01 00:00:00.0",
            lunar_presentation::ResultUtc(
                UtcDateTime::Create(2026, 12, 31, 23, 59, 59, 959)));
  EXPECT_EQ("2026-09-19 00:59:28.5",
            lunar_presentation::ResultUtc(
                UtcDateTime::Create(2026, 9, 19, 0, 59, 28, 491)));
  EXPECT_EQ("A", lunar_presentation::SolutionId(0));
  EXPECT_EQ("Z", lunar_presentation::SolutionId(25));
  EXPECT_EQ("AA", lunar_presentation::SolutionId(26));
}

TEST(LunarPresentation,
     AlmanacAndReductionsUseTheSelectedEpochAndKeepTheObservation) {
  auto sight = Example();
  ASSERT_TRUE(sight.m_LunarSolutionValid) << sight.m_LunarSolutionError;
  const auto inputs = LunarInputSnapshot(sight);
  ASSERT_GE(sight.m_LunarCandidates.size(), 2u);
  const auto retained = sight.m_LunarCalculation;
  const auto ephemeris = sight.m_LunarEphemeris;
  unsigned evaluations = 0;
  sight.m_LunarEphemeris = [&](double offset,
                               lunar_distance::EphemerisSample* sample,
                               std::string* error) {
    ++evaluations;
    return ephemeris(offset, sample, error);
  };
  for (std::size_t index = 0; index < sight.m_LunarCandidates.size(); ++index) {
    evaluations = 0;
    sight.UpdateLunarReport(index);
    EXPECT_LT(evaluations, 30u);  // A report must not rerun the UTC scan.
    EXPECT_EQ(retained, sight.m_LunarCalculation);
    const auto report = lunar_presentation::BuildReport(sight, index);
    EXPECT_TRUE(report.heading.Contains("Solution " +
                                        lunar_presentation::SolutionId(index)));
    const auto* almanac =
        Find(report.almanac, "Geocentric almanac at the lunar-distance epoch");
    ASSERT_NE(nullptr, almanac);
    lunar_distance::EphemerisSample sample;
    std::string error;
    ASSERT_TRUE(ephemeris(sight.m_LunarCandidates[index].offset_seconds,
                          &sample, &error));
    EXPECT_EQ(wxString::Format("%.8f", sample.moon_geographic_latitude_deg),
              almanac->rows[1][1]);
    EXPECT_EQ(wxString::Format("%.8f", sample.body_semidiameter_deg),
              almanac->rows[3][3]);
    const auto* reductions =
        Find(report.almanac, "Direct Triangle comparison: altitude reductions");
    ASSERT_NE(nullptr, reductions);
    EXPECT_EQ("Sextant altitude Hs", reductions->rows.front()[0]);
    EXPECT_EQ("Geocentric altitude Ho", reductions->rows.back()[0]);
    EXPECT_EQ(inputs, LunarInputSnapshot(sight));
  }
}

TEST(LunarPresentation, FittedAgreementAndIndependentSightErrorRemainDistinct) {
  auto sight = Example();
  ASSERT_TRUE(sight.m_LunarSolutionValid);
  const auto original = lunar_distance::CheckDistanceAtPosition(
      sight.LunarObservation(), sight.LunarEphemeris(),
      {sight.m_DRLat, sight.m_DRLon});
  ASSERT_TRUE(original.valid);
  // The Classic example has about a quarter arcminute discrepancy; a fitted
  // residual of zero must not replace this independent check.
  EXPECT_GT(original.lunar_error_arcmin, .2);
  EXPECT_LT(original.lunar_error_arcmin, .3);
  for (std::size_t index = 0; index < sight.m_LunarCandidates.size(); ++index) {
    sight.UpdateLunarReport(index);
    const auto report = lunar_presentation::BuildReport(sight, index);
    const auto* forward =
        Find(report.overview, "Forward check at the selected solution");
    ASSERT_NE(nullptr, forward);
    for (const auto& row : forward->rows)
      EXPECT_LT(std::fabs(wxAtof(row[3])), .001);
    const auto check = lunar_distance::CheckDistanceAtPosition(
        sight.LunarObservation(), sight.LunarEphemeris(),
        {sight.m_DRLat, sight.m_DRLon});
    EXPECT_DOUBLE_EQ(original.lunar_error_arcmin, check.lunar_error_arcmin);
    const auto* refine =
        Find(report.search, "Refinement brackets containing the selected UTC");
    ASSERT_NE(nullptr, refine);
    ASSERT_FALSE(refine->rows.empty());
    for (const auto& row : refine->rows) {
      EXPECT_LE(wxAtof(row[2]),
                sight.m_LunarCandidates[index].offset_seconds + 1e-6);
      EXPECT_GE(wxAtof(row[3]),
                sight.m_LunarCandidates[index].offset_seconds - 1e-6);
    }
  }
}

TEST(LunarPresentation,
     SeparateReadingTimesDoNotShowASimultaneousTriangleReduction) {
  auto sight = Example();
  sight.m_LunarSeparateTimes = true;
  sight.m_LunarMoonTimeOffsetSeconds = -60;
  sight.m_LunarBodyTimeOffsetSeconds = 90;
  const auto inputs = LunarInputSnapshot(sight);
  const auto report = lunar_presentation::BuildReport(sight, -1);
  EXPECT_EQ(nullptr, Find(report.almanac,
                          "Direct Triangle comparison: altitude reductions"));
  const auto* times = Find(report.almanac, "Individual reading times");
  ASSERT_NE(nullptr, times);
  EXPECT_EQ("2026-09-19 00:59:00.000", times->rows[1][1]);
  EXPECT_EQ("2026-09-19 01:01:30.000", times->rows[2][1]);
  EXPECT_EQ(inputs, LunarInputSnapshot(sight));
}
