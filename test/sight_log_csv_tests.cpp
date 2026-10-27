#include <deque>
#include <gtest/gtest.h>

#include <cmath>
#include <locale>

#include "NavigationUIUtils.h"
#include "Sight.h"
#include "SightLogCsv.h"
#include "UtcDateTime.h"

namespace {
// Independent CSV reader: quoted fields can contain commas, quotes and
// newlines.
std::vector<std::deque<wxString>> ReadCsv(const wxString& csv) {
  std::vector<std::deque<wxString>> rows;
  std::deque<wxString> row;
  wxString field;
  bool quoted = false;
  for (size_t i = 0; i < csv.length(); ++i) {
    const wxUniChar c = csv[i];
    if (c == '"') {
      if (quoted && i + 1 < csv.length() && csv[i + 1] == '"') {
        field += '"';
        ++i;
      } else
        quoted = !quoted;
    } else if (!quoted && (c == ',' || c == '\r' || c == '\n')) {
      row.push_back(field);
      field.clear();
      if (c != ',') {
        rows.push_back(row);
        row.clear();
        if (c == '\r' && i + 1 < csv.length() && csv[i + 1] == '\n') ++i;
      }
    } else
      field += c;
  }
  EXPECT_FALSE(quoted);
  EXPECT_TRUE(row.empty());
  EXPECT_TRUE(field.empty());
  return rows;
}

Sight Observation(Sight::Type type = Sight::ALTITUDE) {
  Sight sight(type, "Sun", Sight::LOWER,
              UtcDateTime::Create(2026, 10, 10, 23, 59, 59, 250), 1, 42.25, 0.1,
              true);
  sight.m_DRLat = sight.m_DRLon = NAN;
  return sight;
}
}  // namespace

TEST(SightLogCsv, EscapesUnicodeRemarksAndIncludesHiddenSights) {
  Sight sight = Observation();
  sight.SetVisible(false);
  sight.m_Remarks =
      wxString::FromUTF8("Cloud, spray \"N\" — café\r\nSecond line");
  const auto rows = ReadCsv(SightLogCsv({sight, sight}, 0));
  ASSERT_EQ(3u, rows.size());
  for (const auto& row : rows) ASSERT_EQ(17u, row.size());
  EXPECT_EQ("Number", rows[0][0]);
  EXPECT_EQ("1", rows[1][0]);
  EXPECT_EQ("2", rows[2][0]);
  EXPECT_EQ(sight.m_Remarks, rows[1][16]);
  EXPECT_EQ(sight.m_Remarks, rows[2][16]);
  EXPECT_EQ(1u, ReadCsv(SightLogCsv({}, 0)).size());
}

TEST(SightLogCsv, CorrectsUtcAndExportsUnitsAndPreferredAngles) {
  Sight sight = Observation();
  sight.m_DRLat = 43.95;
  sight.m_DRLon = -78.15;
  sight.m_IndexError = -2.5 / 60.0;
  sight.m_EyeHeight = 3.2;
  sight.m_Temperature = 12.5;
  sight.m_Pressure = 1013.25;
  sight.m_DipShort = true;
  sight.m_AllowDe440 = false;
  const auto rows = ReadCsv(SightLogCsv({sight}, 2));
  ASSERT_EQ(2u, rows.size());
  const auto& row = rows[1];
  ASSERT_EQ(17u, row.size());
  EXPECT_EQ("2026-10-11", row[1]);
  EXPECT_EQ("00:00:01.250", row[2]);
  EXPECT_EQ("Altitude", row[3]);
  EXPECT_EQ("Sun", row[4]);
  EXPECT_EQ("Lower", row[5]);
  EXPECT_EQ(FormatNavigationAngle(42.25), row[6]);
  EXPECT_FALSE(row[7].empty());
  EXPECT_FALSE(row[7].Contains("M"));
  EXPECT_EQ("Dip short", row[8]);
  EXPECT_EQ("3.2", row[9]);
  EXPECT_EQ("-2.5", row[10]);
  EXPECT_EQ("12.5", row[11]);
  EXPECT_EQ("1013.25", row[12]);
  EXPECT_EQ(FormatNavigationAngle(43.95, NavigationAngleKind::Latitude),
            row[13]);
  EXPECT_EQ(FormatNavigationAngle(-78.15, NavigationAngleKind::Longitude),
            row[14]);
  EXPECT_EQ("UTC-5", row[15]);
  EXPECT_EQ(42.25, sight.m_Measurement);
  EXPECT_FALSE(sight.m_CorrectedDateTime.IsValid());
}

TEST(SightLogCsv, LunarReportsObservedDistanceMoonBearingAndSelectedUtc) {
  Sight lunar = Observation(Sight::LUNAR);
  lunar.m_Body = "Jupiter";
  lunar.m_BodyLimb = Sight::LUNAR_FAR;
  lunar.m_DRLat = 43.95;
  lunar.m_DRLon = -78.15;
  lunar.m_AllowDe440 = false;
  lunar.m_LDC = 99;  // Cleared LD must not replace the original observation.
  lunar.m_ArtificialHorizon = true;
  lunar.m_DipShort = true;
  Sight moon = lunar;
  moon.m_Type = Sight::ALTITUDE;
  moon.m_Body = "Moon";
  auto rows = ReadCsv(SightLogCsv({lunar, moon}, 0));
  ASSERT_EQ(3u, rows.size());
  EXPECT_EQ("Lunar", rows[1][3]);
  EXPECT_EQ("Moon / Jupiter", rows[1][4]);
  EXPECT_EQ("Far", rows[1][5]);
  EXPECT_EQ(FormatNavigationAngle(42.25), rows[1][6]);
  EXPECT_EQ(rows[2][7], rows[1][7]);
  EXPECT_EQ("AH", rows[1][8]);
  lunar.m_LunarTimeIsWatch = true;
  rows = ReadCsv(SightLogCsv({lunar}, 0));
  EXPECT_TRUE(rows[1][1].empty());
  EXPECT_TRUE(rows[1][2].empty());
  EXPECT_TRUE(rows[1][7].empty());
  lunar.m_LunarSolutionValid = true;
  lunar.m_LunarCandidates.resize(1);
  lunar.m_LunarCandidates[0].offset_seconds = 61.5;
  lunar.m_LunarSelectedCandidate = 0;
  rows = ReadCsv(SightLogCsv({lunar}, 2));
  EXPECT_EQ("2026-10-11", rows[1][1]);
  EXPECT_EQ("00:01:02.750", rows[1][2]);
  EXPECT_FALSE(rows[1][7].empty());
  EXPECT_EQ(0, lunar.m_LunarSelectedCandidate);
  lunar.m_LunarTimeIsWatch = false;
  rows = ReadCsv(SightLogCsv({lunar}, 2));
  EXPECT_EQ("00:00:01.250", rows[1][2]);
}

TEST(SightLogCsv, EventsAndAzimuthsRetainObservedBearingReference) {
  Sight sunrise = Observation(Sight::HORIZON);
  sunrise.m_HorizonBearingProvided = true;
  sunrise.m_HorizonBearing = 89.5;
  sunrise.m_HorizonBearingMagnetic = true;
  sunrise.m_HorizonVariation = 8;
  Sight sunset = sunrise;
  sunset.m_HorizonEvent = Sight::SUNSET;
  sunset.m_HorizonBearingMagnetic = false;
  sunset.m_HorizonBearing = 270.25;
  Sight timeOnly = sunrise;
  timeOnly.m_HorizonBearingProvided = false;
  Sight azimuth = Observation(Sight::AZIMUTH);
  azimuth.m_bMagneticNorth = true;
  auto rows = ReadCsv(SightLogCsv({sunrise, sunset, timeOnly, azimuth}, 0));
  ASSERT_EQ(5u, rows.size());
  EXPECT_EQ("Sunrise", rows[1][3]);
  EXPECT_EQ("Sunset", rows[2][3]);
  EXPECT_EQ("Sun", rows[1][4]);
  EXPECT_TRUE(rows[1][5].empty());
  EXPECT_EQ("0", rows[1][6]);
  EXPECT_EQ("89.5 M", rows[1][7]);
  EXPECT_EQ("270.25", rows[2][7]);
  EXPECT_TRUE(rows[3][7].empty());
  EXPECT_EQ("Azimuth", rows[4][3]);
  EXPECT_EQ("42.25 M", rows[4][7]);
}

TEST(SightLogCsv, MissingDataStaysBlankAndZoneUsesLongitudeSign) {
  Sight sight = Observation();
  sight.m_DateTime = wxDateTime();
  sight.m_Measurement = NAN;
  sight.m_EyeHeight = INFINITY;
  sight.m_DRLat = 91;
  sight.m_DRLon = 181;
  auto rows = ReadCsv(SightLogCsv({sight}, 0));
  for (int column : {1, 2, 6, 7, 9, 13, 14, 15})
    EXPECT_TRUE(rows[1][column].empty()) << column;
  sight.m_DRLon = 37.5;
  rows = ReadCsv(SightLogCsv({sight}, 0));
  EXPECT_EQ("UTC+3", rows[1][15]);
  sight.m_DRLon = -37.5;
  rows = ReadCsv(SightLogCsv({sight}, 0));
  EXPECT_EQ("UTC-3", rows[1][15]);
  sight.m_DRLon = 0;
  rows = ReadCsv(SightLogCsv({sight}, 0));
  EXPECT_EQ("UTC+0", rows[1][15]);
}

TEST(SightLogCsv, NumericFieldsUseDecimalPointInCommaLocales) {
  struct CommaDecimal : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
  };
  const auto original = std::locale();
  std::locale::global(std::locale(original, new CommaDecimal));
  Sight sight = Observation();
  sight.m_EyeHeight = 2.75;
  const wxString csv = SightLogCsv({sight}, 0);
  std::locale::global(original);
  const auto rows = ReadCsv(csv);
  ASSERT_EQ(2u, rows.size());
  EXPECT_EQ("2.75", rows[1][9]);
}
