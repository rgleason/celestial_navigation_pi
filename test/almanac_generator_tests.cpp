#include <gtest/gtest.h>

#include "AlmanacGenerator.h"
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/utils.h>

#include <chrono>
#include <algorithm>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {
wxDateTime Fields(const char* value) {
  wxDateTime result;
  EXPECT_TRUE(result.ParseISOCombined(value));
  return result;
}

AlmanacRequest Request(unsigned days = 3) {
  AlmanacRequest request;
  AlmanacGenerator::ApplyPreset(AlmanacPreset::VoyageAlmanac, &request);
  request.fromUtc = Fields("2026-08-16T00:00:00");
  request.toUtc = UtcDateTime::AddSeconds(request.fromUtc,
                                          (days - 1) * 86400.0);
  request.coverage = AlmanacCoverage::FixedPosition;
  request.latitude = 53.19;
  request.longitude = -2.89;
  request.voyageName = "Regression Voyage";
  return request;
}
}  // namespace

TEST(AlmanacDependencies, CalculatorCompleteRestoresEssentialMaterial) {
  AlmanacRequest request = Request();
  request.includeSun = false;
  request.includeMoon = false;
  request.includeAries = false;
  request.includeCorrections = false;
  request.includeInstructions = false;
  request.includeEmergencyGuide = false;
  wxString error;
  ASSERT_TRUE(AlmanacGenerator::Validate(&request, &error));
  EXPECT_TRUE(request.includeSun);
  EXPECT_TRUE(request.includeMoon);
  EXPECT_TRUE(request.includeAries);
  EXPECT_TRUE(request.includeCorrections);
  EXPECT_TRUE(request.includeInstructions);
  EXPECT_TRUE(request.includeEmergencyGuide);
}

TEST(AlmanacAccuracy, MatchesStoredUsnoNavigationalPlanetFixture) {
  // USNO Celestial Navigation Data, 2025-05-04 13:00:00 UT1,
  // AP 36.3352 S, 121.7511 E: Mars GHA 287 deg 30.5 min,
  // Dec N 20 deg 19.5 min, Hc +16 deg 41.4 min, Zn 312.1 deg.
  const BodyState mars = CelestialEphemeris::Evaluate(
      "Mars", UtcDateTime::ToInstant(Fields("2025-05-04T13:00:00")),
      -36.3352, 121.7511);
  ASSERT_TRUE(mars.valid);
  EXPECT_NEAR(287.0 + 30.5 / 60.0, mars.gha, 0.04);
  EXPECT_NEAR(20.0 + 19.5 / 60.0, mars.declination, 0.04);
  EXPECT_NEAR(16.0 + 41.4 / 60.0, mars.geometricAltitude, 0.08);
  EXPECT_NEAR(312.1, mars.azimuthTrue, 0.15);
}

TEST(AlmanacDependencies, RejectsLongOrBackwardGateSixRanges) {
  AlmanacRequest request = Request(1);
  wxString error;
  request.toUtc = Fields("2026-08-15T00:00:00");
  EXPECT_FALSE(AlmanacGenerator::Validate(&request, &error));
  request.toUtc = Fields("2026-11-30T00:00:00");
  EXPECT_FALSE(AlmanacGenerator::Validate(&request, &error));
}

TEST(AlmanacRoute, InterpolatesByDistanceAndCrossesDatelineShortWay) {
  AlmanacRequest request = Request(3);
  request.coverage = AlmanacCoverage::PlannedRoute;
  AlmanacRoutePoint a;
  a.latitude = 10;
  a.longitude = 170;
  AlmanacRoutePoint b;
  b.latitude = 10;
  b.longitude = -170;
  request.route.push_back(a);
  request.route.push_back(b);
  request.routeSpeedKnots = 24.63;  // approximately half the leg per day
  const AlmanacRoutePoint midpoint =
      AlmanacGenerator::PositionForDay(request, 1, 3);
  EXPECT_NEAR(10.0, midpoint.latitude, 0.01);
  EXPECT_NEAR(180.0, std::fabs(midpoint.longitude), 0.01);
}

TEST(AlmanacDocument, UsesExactSemanticPagesAndUniversalHourlyRows) {
  const AlmanacRequest request = Request(3);
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  ASSERT_GT(document.pages.size(), 10u);
  EXPECT_EQ("Regression Voyage", document.pages.front().title);
  unsigned ephemerisPages = 0;
  unsigned universalPages = 0;
  unsigned planetPages = 0;
  unsigned starRows = 0;
  for (const AlmanacPage& page : document.pages) {
    if (page.section == "Daily ephemeris") {
      ++ephemerisPages;
      ASSERT_FALSE(page.tables.empty());
      EXPECT_EQ(24u, page.tables.front().rows.size());
      if (page.title.Find("navigational planets") == wxNOT_FOUND)
        ++universalPages;
      else
        ++planetPages;
    }
    if (page.section == "Star data") {
      ASSERT_FALSE(page.tables.empty());
      starRows += page.tables.front().rows.size();
    }
  }
  EXPECT_EQ(6u, ephemerisPages);
  EXPECT_EQ(3u, universalPages);
  EXPECT_EQ(3u, planetPages);
  EXPECT_EQ(57u, starRows);
  EXPECT_EQ((document.pages.size() + 1) / 2, document.sheets);
  EXPECT_NE(wxNOT_FOUND, document.manifest.Find("DUT1"));
  EXPECT_EQ(document.pages.size(), AlmanacGenerator::Estimate(request).pages.size());
}

TEST(AlmanacDocument, EstimateRemainsExactForIndependentReferenceChoices) {
  AlmanacRequest request = Request(1);
  request.safety = AlmanacSafety::PlanningReference;
  request.selfContained = false;
  request.includeInstructions = false;
  request.includeCorrections = true;
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  EXPECT_EQ(document.pages.size(), AlmanacGenerator::Estimate(request).pages.size());
  EXPECT_NE(std::find_if(document.pages.begin(), document.pages.end(),
                         [](const AlmanacPage& page) {
                           return page.title == "Altitude corrections";
                         }),
            document.pages.end());
}

TEST(AlmanacDocument, DatesStayOnConsecutiveUtcDaysAcrossDstChange) {
  AlmanacRequest request = Request(4);
  request.fromUtc = Fields("2026-10-24T00:00:00");
  request.toUtc = Fields("2026-10-27T00:00:00");
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  wxString titles;
  for (const AlmanacPage& page : document.pages)
    if (page.section == "Daily ephemeris") titles += page.title + "\n";
  EXPECT_NE(wxNOT_FOUND, titles.Find("24 October 2026"));
  EXPECT_NE(wxNOT_FOUND, titles.Find("25 October 2026"));
  EXPECT_NE(wxNOT_FOUND, titles.Find("26 October 2026"));
  EXPECT_NE(wxNOT_FOUND, titles.Find("27 October 2026"));
}

TEST(AlmanacPdf, ProducesACompletePdfWithOnePageObjectPerModelPage) {
  const AlmanacRequest request = Request(2);
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  wxString output;
  const bool keep = wxGetEnv("CELESTIAL_TEST_PDF", &output) && !output.empty();
  if (!keep) output = wxFileName::CreateTempFileName("celestial-almanac-");
  wxString error;
  ASSERT_TRUE(AlmanacPdfWriter::Write(document, request, output, &error))
      << error;
  wxFile file(output);
  ASSERT_TRUE(file.IsOpened());
  wxString bytes;
  ASSERT_TRUE(file.ReadAll(&bytes, wxConvISO8859_1));
  EXPECT_TRUE(bytes.StartsWith("%PDF-1.4"));
  EXPECT_NE(wxNOT_FOUND, bytes.Find("%%EOF"));
  EXPECT_NE(wxNOT_FOUND, bytes.Find(
      wxString::Format("/Count %u", static_cast<unsigned>(document.pages.size()))));
  file.Close();
  if (!keep) wxRemoveFile(output);
}

TEST(AlmanacPerformance, ReportsGateSixMaximumAssemblyTime) {
  AlmanacRequest request = Request(93);
  const auto started = std::chrono::steady_clock::now();
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  const wxString output = wxFileName::CreateTempFileName("celestial-benchmark-");
  const auto pdfStarted = std::chrono::steady_clock::now();
  wxString error;
  ASSERT_TRUE(AlmanacPdfWriter::Write(document, request, output, &error));
  const auto pdfElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - pdfStarted);
  const wxULongLong pdfBytes = wxFileName(output).GetSize();
  RecordProperty("days", 93);
  RecordProperty("pages", static_cast<int>(document.pages.size()));
  RecordProperty("assembly_ms", static_cast<int>(elapsed.count()));
  RecordProperty("pdf_ms", static_cast<int>(pdfElapsed.count()));
  RecordProperty("pdf_bytes", static_cast<long long>(pdfBytes.GetValue()));
#if defined(__unix__) || defined(__APPLE__)
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0)
    RecordProperty("process_peak_rss_kb", static_cast<long long>(usage.ru_maxrss));
#endif
  EXPECT_LT(elapsed.count(), 120000);
  wxRemoveFile(output);
}

TEST(AlmanacPerformance, ReportsFortnightAssemblyAndPdfTime) {
  AlmanacRequest request = Request(14);
  const auto started = std::chrono::steady_clock::now();
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  const wxString output = wxFileName::CreateTempFileName("celestial-benchmark-");
  const auto pdfStarted = std::chrono::steady_clock::now();
  wxString error;
  ASSERT_TRUE(AlmanacPdfWriter::Write(document, request, output, &error));
  const auto pdfElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - pdfStarted);
  RecordProperty("days", 14);
  RecordProperty("pages", static_cast<int>(document.pages.size()));
  RecordProperty("assembly_ms", static_cast<int>(elapsed.count()));
  RecordProperty("pdf_ms", static_cast<int>(pdfElapsed.count()));
  RecordProperty("pdf_bytes",
                 static_cast<long long>(wxFileName(output).GetSize().GetValue()));
  wxRemoveFile(output);
}
