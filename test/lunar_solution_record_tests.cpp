#include <gtest/gtest.h>
#include "LunarSolutionRecord.h"

TEST(LunarSolutionRecord, NumericAttributesRetainRawAnglePrecision) {
  TiXmlElement sight("Sight");
  const double recorded = 85.67166666666667;
  SetPreciseXmlDouble(&sight, "Measurement", recorded);
  double loaded = 0;
  ASSERT_TRUE(ReadXmlDouble(&sight, "Measurement", &loaded));
  EXPECT_DOUBLE_EQ(recorded, loaded);
}

TEST(LunarSolutionRecord, OldClockNodeStillLoadsWithoutDerivedRecords) {
  TiXmlDocument doc;
  doc.Parse("<ClockError Seconds='17'/>");
  EXPECT_TRUE(ReadLunarSolutions(doc.RootElement()).empty());
  EXPECT_STREQ("17", doc.RootElement()->Attribute("Seconds"));
}

TEST(LunarSolutionRecord,
     RoundTripPreservesRawSnapshotsAndFractionalCorrection) {
  TiXmlDocument doc;
  auto* clock = new TiXmlElement("ClockError");
  clock->SetAttribute("Seconds", 17);
  doc.LinkEndChild(clock);
  LunarSolutionRecord record;
  record.name = wxString::FromUTF8("Paul's watch – Moon & Sun");
  record.reference_time = "2024-06-13 19:26:00";
  record.base_correction_seconds = 17;
  record.additional_correction_seconds = -8.4521484375;
  record.time_sigma_seconds = 26.3;
  record.inputs = {"raw: LD=85.67166666666667 <unchanged>"};
  record.report = "Candidate 2\nWGS84 <model> & immutable readings";
  WriteLunarSolutions(clock, {record});
  TiXmlPrinter printer;
  doc.Accept(&printer);
  TiXmlDocument loaded;
  loaded.Parse(printer.CStr());
  ASSERT_FALSE(loaded.Error());
  const auto records = ReadLunarSolutions(loaded.RootElement());
  ASSERT_EQ(1u, records.size());
  EXPECT_EQ(record.name, records[0].name);
  EXPECT_EQ(record.report, records[0].report);
  EXPECT_EQ(record.inputs, records[0].inputs);
  EXPECT_NEAR(record.TotalCorrection(), records[0].TotalCorrection(), 1e-12);
  EXPECT_STREQ("17", loaded.RootElement()->Attribute("Seconds"));
}

TEST(LunarSolutionRecord, FixUsesWorkingCopiesAndAppliesTotalCorrectionOnce) {
  wxDateTime time;
  ASSERT_TRUE(time.ParseISOCombined("2024-06-13T19:26:00"));
  Sight sight(Sight::ALTITUDE, "Sun", Sight::LOWER, time, 0, 51.9, 0.2);
  sight.m_bVisible = true;
  const std::vector<Sight> recorded{sight};
  LunarSolutionRecord solution;
  solution.reference_time = "2024-06-13 19:26:00";
  solution.base_correction_seconds = 17;
  solution.additional_correction_seconds = -8.452;
  std::vector<Sight> working;
  wxString error;
  ASSERT_TRUE(PrepareLunarFixSights(
      recorded, &solution, solution.TotalCorrection(), &working, &error))
      << error;
  ASSERT_EQ(1u, working.size());
  EXPECT_DOUBLE_EQ(sight.m_Measurement, working[0].m_Measurement);
  EXPECT_EQ(sight.m_DateTime, working[0].m_DateTime);
  EXPECT_NEAR(UtcDateTime::SecondsBetween(working[0].m_CorrectedDateTime, time),
              8.548, 0.001);
  EXPECT_EQ(time, recorded[0].m_DateTime);
  EXPECT_EQ(sight.m_CorrectedDateTime, recorded[0].m_CorrectedDateTime);
  auto another_watch = recorded;
  another_watch[0].m_DateTime = UtcDateTime::AddSeconds(time, 86400);
  EXPECT_FALSE(PrepareLunarFixSights(
      another_watch, &solution, solution.TotalCorrection(), &working, &error));
  EXPECT_TRUE(working.empty());
  EXPECT_FALSE(error.empty());
}
