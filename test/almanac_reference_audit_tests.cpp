// Opt-in, offline audit of official USNO reference fixtures. The companion
// fetch script is run explicitly; ordinary tests never access the network.
#include <gtest/gtest.h>

#include "AlmanacGenerator.h"
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"
#include "eclipse/dut1.h"

#include <wx/init.h>
#include <wx/utils.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
wxDateTime Utc(const std::string& iso) {
  wxDateTime result;
  EXPECT_TRUE(result.ParseISOCombined(wxString::FromUTF8(iso)));
  return result;
}
std::vector<std::string> Split(const std::string& line) {
  std::vector<std::string> fields;
  std::istringstream input(line);
  std::string field;
  while (std::getline(input, field, '\t')) fields.push_back(field);
  return fields;
}
double Angle(const wxString& text) {
  wxString input = text;
  double sign = 1;
  if (input.StartsWith("S ")) { sign = -1; input = input.Mid(2); }
  if (input.StartsWith("N ")) input = input.Mid(2);
  long whole = 0;
  double minutes = 0;
  EXPECT_TRUE(input.BeforeFirst(' ').ToLong(&whole));
  wxString minute = input.AfterFirst(' ');
  minute.Replace("'", "");
  EXPECT_TRUE(minute.ToDouble(&minutes));
  return sign * (whole + minutes / 60.0);
}
const std::vector<wxString>* PrintedRow(const AlmanacDocument& document,
                                         const wxString& body, int hour,
                                         int* gha, int* dec) {
  for (const auto& page : document.pages) {
    if (page.section != "Daily ephemeris") continue;
    if (body == "Aries" || body == "Sun") {
      if (page.title.Find("planets") != wxNOT_FOUND) continue;
      *gha = body == "Aries" ? 1 : 2;
      *dec = body == "Aries" ? -1 : 3;
      return &page.tables.at(0).rows.at(hour);
    }
    if (body == "Moon") {
      if (page.title.Find("planets") != wxNOT_FOUND) continue;
      *gha = 1; *dec = 3;
      return &page.tables.at(1).rows.at(hour);
    }
    if (page.title.Find("planets") == wxNOT_FOUND) continue;
    for (const auto& table : page.tables)
      for (int col : {1, 6})
        if (table.headings.at(col) == body + " GHA") {
          *gha = col; *dec = col + 2;
          return &table.rows.at(hour);
        }
  }
  return nullptr;
}
wxString PrintedStarSha(const AlmanacDocument& document, const wxString& body) {
  for (const auto& page : document.pages) {
    if (page.section != "Star data") continue;
    for (const auto& table : page.tables)
      for (const auto& row : table.rows)
        if (row.size() >= 4 && row[1].CmpNoCase(body) == 0)
          return row[2];
  }
  return wxString();
}
AlmanacDocument OneDay(const wxDateTime& utc) {
  AlmanacRequest request;
  AlmanacGenerator::ApplyPreset(AlmanacPreset::CompactAstronavigation, &request);
  request.fromUtc = request.toUtc =
      wxDateTime(utc.GetDay(), utc.GetMonth(), utc.GetYear());
  // Paper references do not alter the hourly ephemeris. Avoid regenerating
  // more than a hundred identical paper pages for each audited day.
  request.safety = AlmanacSafety::CalculatorComplete;
  request.includeIncrementTables = false;
  request.includeCompactReductionTables = false;
  request.includeAltitudeCorrectionTables = false;
  request.monthlyStarData = false;
  request.sightForms = request.runningFixForms = request.noonPolarisForms = 0;
  return AlmanacGenerator::Build(request);
}
}  // namespace

TEST(AlmanacReferenceAudit, ExportUtcAndDut1Schedule) {
  wxString path;
  if (!wxGetEnv("CELNAV_REFERENCE_SCHEDULE", &path)) GTEST_SKIP();
  wxInitializer init;
  ASSERT_TRUE(init.IsOk());
  std::ofstream output(path.ToStdString());
  ASSERT_TRUE(output.good());
  output << "utc\tut1_date\tut1_time\tdut1_seconds\tdut1_quality\tlatitude\tlongitude\n";
  const char* dates[] = {
      "2015-06-30", "2015-07-01", "2024-01-03", "2024-03-20",
      "2024-06-13", "2024-09-22", "2025-05-04", "2026-01-03",
      "2026-03-20", "2026-06-21", "2026-08-16", "2027-08-01"};
  const double sites[][2] = {{0.0, 0.0}, {41.3666667, -71.4833333},
                              {-34.6, 149.0}};
  unsigned samples = 0;
  for (const char* date : dates)
    for (int hour : {0, 8, 16})
      for (const auto& site : sites) {
        const wxDateTime utc = Utc(std::string(date) +
            (hour == 0 ? "T00:00:00" : hour == 8 ? "T08:00:00" : "T16:00:00"));
        const auto dut1 = eclipse::LookupDut1(
            UtcDateTime::ToInstant(utc).GetJulianDayNumber());
        ASSERT_TRUE(dut1.available) << date;
        const wxDateTime ut1 = UtcDateTime::AddSeconds(utc, dut1.seconds);
        output << utc.FormatISOCombined('T').ToStdString() << '\t'
               << ut1.FormatISODate().ToStdString() << '\t'
               << wxString::Format("%02d:%02d:%09.6f", ut1.GetHour(),
                       ut1.GetMinute(), ut1.GetSecond() +
                       ut1.GetMillisecond() / 1000.0).ToStdString() << '\t'
               << std::setprecision(9) << dut1.seconds << '\t'
               << dut1.quality << '\t' << site[0] << '\t' << site[1] << '\n';
        ++samples;
      }
  EXPECT_EQ(samples, 108u);
}

TEST(AlmanacReferenceAudit, CompareStoredUsnoRowsAndPrintedAlmanac) {
  wxString fixture, outputPath;
  if (!wxGetEnv("CELNAV_REFERENCE_FIXTURE", &fixture) ||
      !wxGetEnv("CELNAV_REFERENCE_OUTPUT", &outputPath)) GTEST_SKIP();
  wxInitializer init;
  ASSERT_TRUE(init.IsOk());
  std::ifstream input(fixture.ToStdString());
  std::ofstream output(outputPath.ToStdString());
  ASSERT_TRUE(input.good() && output.good());
  output << "utc\tut1\tdut1_seconds\tlatitude\tlongitude\tbody\tmode\tengine_source"
            "\tref_gha\tmodel_gha\tprinted_gha\tref_dec\tmodel_dec\tprinted_dec"
            "\tref_hc\tmodel_hc\tref_zn\tmodel_zn\tref_sd\tmodel_geo_sd"
            "\tprinted_sha\tapi_version\turl\n";
  std::string line, lastDay;
  std::getline(input, line);  // header
  AlmanacDocument documents[2];
  unsigned rows = 0;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto fields = Split(line);
    ASSERT_EQ(fields.size(), 13u) << line;
    const wxDateTime utc = Utc(fields[0]);
    const wxString body = wxString::FromUTF8(fields[5]);
    const std::string day = fields[0].substr(0, 10);
    if (day != lastDay) {
      lastDay = day;
      for (int mode = 0; mode != 2; ++mode) {
        if (mode) wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
        else wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
        documents[mode] = OneDay(utc);
      }
    }
    for (int mode = 0; mode != 2; ++mode) {
      if (mode) wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
      else wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
      const double dut1 = std::stod(fields[2]);
      const double latitude = std::stod(fields[3]);
      const double longitude = std::stod(fields[4]);
      const BodyState state = CelestialEphemeris::Evaluate(
          body == "Aries" ? "Sun" : body, UtcDateTime::ToInstant(utc),
          latitude, longitude,
          1010.0, 10.0, dut1);
      ASSERT_TRUE(state.valid) << body << " at " << fields[0];
      if (body == "Sun" || body == "Moon" || body == "Venus")
        EXPECT_EQ(state.usedDe440, mode == 1) << body << " at " << fields[0];
      else if (body != "Aries")
        EXPECT_FALSE(state.usedDe440) << body << " at " << fields[0];
      int ghaCol = -1, decCol = -1;
      const auto* printed = PrintedRow(documents[mode], body, utc.GetHour(),
                                       &ghaCol, &decCol);
      const double modelGha = body == "Aries" ? state.ghaAries : state.gha;
      if (printed) {
        const double separation = std::remainder(
            Angle(printed->at(ghaCol)) - modelGha, 360.0) * 60.0;
        EXPECT_LE(std::fabs(separation), 0.050001)
            << body << " printed GHA at " << fields[0];
        if (decCol >= 0)
          EXPECT_LE(std::fabs(Angle(printed->at(decCol)) -
                              state.declination) * 60.0, 0.050001)
              << body << " printed declination at " << fields[0];
      }
      output << fields[0] << '\t' << fields[1] << '\t' << fields[2] << '\t'
             << fields[3] << '\t' << fields[4] << '\t' << fields[5] << '\t'
             << (mode ? "DE440s enabled" : "Analytical") << '\t'
             << (body == "Aries" ? "Earth rotation" :
                 state.usedDe440 ? "DE440s" : "Analytical") << '\t'
             << fields[6] << '\t' << std::setprecision(12) << modelGha << '\t';
      if (printed) output << Angle(printed->at(ghaCol));
      output << '\t' << fields[7] << '\t';
      if (body != "Aries") output << state.declination;
      output << '\t';
      if (printed && decCol >= 0) output << Angle(printed->at(decCol));
      output << '\t' << fields[8] << '\t';
      if (body != "Aries") output << state.geometricAltitude;
      output << '\t' << fields[9] << '\t';
      if (body != "Aries") output << state.azimuthTrue;
      output << '\t' << fields[10] << '\t';
      if (body != "Aries") output << state.geocentricSemidiameter;
      output << '\t';
      if (utc.GetHour() == 0) output << PrintedStarSha(documents[mode], body);
      output << '\t' << fields[11] << '\t' << fields[12] << '\n';
      ++rows;
    }
  }
  EXPECT_GT(rows, 300u);
}
