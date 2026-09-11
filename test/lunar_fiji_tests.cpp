#include <gtest/gtest.h>
#include <limits>
#include "LunarCandidateSelection.h"
#include "Sight.h"

TEST(LunarCandidateSelection, MissingInvalidDRAndExplicitOverride) {
  std::vector<lunar_distance::TimeCandidate> roots(2);
  roots[0].offset_seconds = 3;
  roots[0].positions = {{18,154}};
  roots[1].offset_seconds = 7;
  roots[1].positions = {{-20,179.75}};
  const lunar_distance::GeographicPoint dr{-20,179.75};
  EXPECT_EQ(0, lunar_distance::SelectLunarCandidate(roots));
  EXPECT_EQ(1, lunar_distance::SelectLunarCandidate(roots,&dr));
  EXPECT_EQ(0, lunar_distance::SelectLunarCandidate(roots,&dr,0));
  EXPECT_EQ(1, lunar_distance::SelectLunarCandidate(roots,&dr,99));
  for (const auto& invalid : std::vector<lunar_distance::GeographicPoint>{
           {91,0}, {0,181}, {std::numeric_limits<double>::quiet_NaN(),0}})
    EXPECT_EQ(0, lunar_distance::SelectLunarCandidate(roots,&invalid));
  roots[1].positions = {{91,179.75}};
  EXPECT_EQ(0, lunar_distance::SelectLunarCandidate(roots,&dr));
  roots[0].positions.clear(); roots[1].positions.clear();
  EXPECT_EQ(0, lunar_distance::SelectLunarCandidate(roots,&dr));
  EXPECT_EQ(-1, lunar_distance::SelectLunarCandidate({}));
}

TEST(LunarCandidateSelection, DateLineAndMultiplePositions) {
  std::vector<lunar_distance::TimeCandidate> roots(2);
  roots[0].offset_seconds = 1; roots[0].positions = {{-20,170}};
  roots[1].offset_seconds = 10; roots[1].positions = {{18,154},{-20,-179.9}};
  const lunar_distance::GeographicPoint dr{-20,179.9};
  EXPECT_EQ(1, lunar_distance::SelectLunarCandidate(roots,&dr));
}

// Bob's PDF: Lunar.Distance.Use.Case.7b.Saturn.Full.Moon.v.2.8.5.2.Regression.PDF
// https://github.com/user-attachments/files/32082181/Lunar.Distance.Use.Case.7b.Saturn.Full.Moon.v.2.8.5.2.Regression.PDF.pdf
// Use screenshot Hs 43deg58', not the contradictory worksheet decimal 44.0833.
// 7a changes only the distance: 7b deliberately adds 1.5' of error.
TEST(LunarFiji, ScreenshotAndWorksheetChooseSouthernBranch) {
  for (const bool worksheet : {false,true}) {
    for (const double minutes : {24.9,26.4}) {
      SCOPED_TRACE(::testing::Message() << "worksheet=" << worksheet << " LD minutes=" << minutes);
      wxDateTime utc(5,wxDateTime::Nov,2025,10,0,0);
      Sight sight(Sight::LUNAR,"Saturn",Sight::LUNAR_NEAR,utc,5400,
                  46+minutes/60,0.2);
      sight.m_DRLat=-20; sight.m_DRLon=179.75;
      sight.m_DRBoatPosition=false;
      sight.m_EyeHeight=worksheet ? 2.44 : 2.40;
      sight.m_Pressure=worksheet ? 1015.9 : 1015.0;
      sight.m_Temperature=21.1; sight.m_IndexError=0.1;
      sight.m_DipShort=false; sight.m_ArtificialHorizon=false;
      sight.m_LunarMoonAltitude=43+58.0/60;
      sight.m_LunarBodyAltitude=66.5;
      sight.m_LunarMoonLimb=Sight::LOWER;
      sight.m_LunarBodyLimb=Sight::LOWER;
      sight.m_LunarMoonAltitudeUncertainty=0.5;
      sight.m_LunarBodyAltitudeUncertainty=0.5;
      sight.Recompute(0);
      ASSERT_TRUE(sight.m_LunarSolutionValid);
      ASSERT_EQ(2u,sight.m_LunarCandidates.size());
      EXPECT_EQ(1,sight.m_LunarSelectedCandidate);
      ASSERT_GE(sight.m_LunarSelectedPosition,0);
      const auto p=sight.m_LunarPositionResult.candidates.at(sight.m_LunarSelectedPosition);
      const lunar_distance::GeographicPoint expected{-20,minutes==24.9 ? 179.75 : 179.15};
      EXPECT_LT(p.latitude_deg,0);
      EXPECT_LT(lunar_distance::GreatCircleDistanceNm(expected,p),minutes==24.9 ? 1.0 : 2.0);
      EXPECT_NEAR(sight.m_LunarCandidates[1].offset_seconds,minutes==24.9 ? 7.046 : 148.198,0.2);
      EXPECT_FALSE(sight.m_LunarUsesDe440); // Saturn path, not Sun-Moon audit.
      const auto prediction=lunar_distance::PredictTimeTaggedObservation(
          sight.LunarObservation(),sight.LunarEphemeris(),
          sight.m_LunarCandidates[1].offset_seconds,p);
      ASSERT_TRUE(prediction.valid);
      EXPECT_NEAR(prediction.raw_distance_deg,sight.m_Measurement,0.001/60);
      EXPECT_NEAR(prediction.moon_altitude_deg,sight.m_LunarMoonAltitude,0.001/60);
      EXPECT_NEAR(prediction.body_altitude_deg,sight.m_LunarBodyAltitude,0.001/60);
      sight.RecomputeLunar(0);
      EXPECT_EQ(0,sight.m_LunarSelectedCandidate);
      EXPECT_GT(sight.m_LunarPositionResult.candidates.at(sight.m_LunarSelectedPosition).latitude_deg,0);
      EXPECT_EQ(0,sight.SelectLunarCandidate(sight.m_LunarSelectedCandidate));
      EXPECT_DOUBLE_EQ(46+minutes/60,sight.m_Measurement);
      EXPECT_EQ(utc,sight.m_DateTime);
      sight.m_DRLat=0; sight.m_DRLon=0;
      EXPECT_EQ(0,sight.SelectLunarCandidate()); // Legacy unset DR must not rank branches.
      sight.m_DRLat=std::numeric_limits<double>::quiet_NaN();
      EXPECT_EQ(0,sight.SelectLunarCandidate());
    }
  }
}
