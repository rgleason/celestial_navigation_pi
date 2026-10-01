#include <gtest/gtest.h>

#include "AlmanacGenerator.h"
#include "AlmanacPaperTables.h"
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <wx/init.h>

#include <chrono>
#include <algorithm>
#include <map>
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

namespace {
struct AlmanacKernelScope {
  wxString previous;
  bool hadPrevious = wxGetEnv("CELNAV_TEST_DE440_ENABLE", &previous);
  AlmanacKernelScope() { wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1"); }
  ~AlmanacKernelScope() {
    if (hadPrevious) wxSetEnv("CELNAV_TEST_DE440_ENABLE", previous);
    else wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
  }
};
double PrintedNumber(wxString text) {
  text.Replace("'", "");
  double result = 0;
  EXPECT_TRUE(text.ToDouble(&result)) << text;
  return result;
}
double PrintedAngle(wxString text) {
  const double sign = text.StartsWith("S ") ? -1.0 : 1.0;
  text.Replace("N ", ""); text.Replace("S ", "");
  return sign * (PrintedNumber(text.BeforeFirst(' ')) +
                 PrintedNumber(text.AfterFirst(' ')) / 60.0);
}
}  // namespace

TEST(AlmanacAccuracy, PrintedSunReductionUsesPublishedReferenceAndPaperCorrections) {
  wxInitializer initializer;
  ASSERT_TRUE(initializer.IsOk());
  AlmanacKernelScope kernel;
  auto request = Request(1);
  AlmanacGenerator::ApplyPreset(AlmanacPreset::CompactAstronavigation, &request);
  request.fromUtc = request.toUtc = Fields("2024-06-13T00:00:00");
  const auto document = AlmanacGenerator::Build(request);
  const AlmanacTable* sun = nullptr;
  const AlmanacTable* increment = nullptr;
  const AlmanacTable* dip = nullptr;
  const AlmanacTable* refraction = nullptr;
  for (const auto& page : document.pages) {
    if (page.section == "Daily ephemeris" &&
        page.title.Find("planets") == wxNOT_FOUND) sun = &page.tables.at(0);
    if (page.title.StartsWith("Minute 26 -")) increment = &page.tables.at(0);
    if (page.title == "Dip of the sea horizon") dip = &page.tables.at(0);
    if (page.title == "Refraction and atmosphere multiplier") refraction = &page.tables.at(0);
  }
  ASSERT_NE(sun, nullptr); ASSERT_NE(increment, nullptr);
  ASSERT_NE(dip, nullptr); ASSERT_NE(refraction, nullptr);
  // Work with rounded, printed cells, not unrounded engine values.
  const auto& hour = sun->rows.at(19);
  const double gha = std::fmod(PrintedAngle(hour.at(2)) +
      PrintedAngle(increment->rows.at(0).at(1)), 360.0);
  const double dec = PrintedAngle(hour.at(3)) +
      PrintedNumber(hour.at(4)) * (26.0 / 60.0) / 60.0;
  // Archived USNO API result; see NavigationDe440.MatchesUsnoPublishedPrecisionAtPointJudith.
  constexpr double referenceGha = 111.434162, referenceDec = 23.267001;
  EXPECT_NEAR(gha, referenceGha, 0.2 / 60.0);
  EXPECT_NEAR(dec, referenceDec, 0.1 / 60.0);
  const auto reduction = AlmanacPaperTables::ReduceAgeton(
      41.3666667, dec, gha - 71.4833333, true);
  ASSERT_TRUE(reduction.valid);
  const double rad = std::acos(-1.0) / 180.0;
  const double lat = 41.3666667 * rad, decl = referenceDec * rad;
  const double lha = (referenceGha - 71.4833333) * rad;
  const double referenceHc = std::asin(std::sin(lat) * std::sin(decl) +
      std::cos(lat) * std::cos(decl) * std::cos(lha)) / rad;
  const double referenceZn = std::fmod(540.0 + std::atan2(std::sin(lha),
      std::cos(lha) * std::sin(lat) - std::tan(decl) * std::cos(lat)) / rad, 360.0);
  EXPECT_NEAR(reduction.computedAltitude, referenceHc, 0.5 / 60.0);
  EXPECT_NEAR(reduction.azimuthTrue, referenceZn, 0.1);
  // Synthetic Sun LL Hs 60 deg 03.5', eye 4 m, IC zero, standard atmosphere.
  double printedDip = -1, printedR = -1;
  for (const auto& row : dip->rows)
    for (size_t col = 0; col + 1 < row.size(); col += 2)
      if (row[col] == "4.0") printedDip = PrintedNumber(row[col + 1]);
  for (const auto& row : refraction->rows)
    for (size_t col = 0; col + 1 < row.size(); col += 2)
      if (row[col] == "60.0") printedR = PrintedNumber(row[col + 1]);
  ASSERT_GE(printedDip, 0); ASSERT_GE(printedR, 0);
  const double ho = 60 + (3.5 - printedDip - printedR +
      PrintedNumber(hour.at(6)) + 0.1) / 60.0;
  // Independent Bennett refraction and first-order solar parallax at Ha=60.
  const double r = 1.02 / std::tan((60 + 10.3 / 65.11) * rad);
  const double referenceHo = 60 + (3.5 - 1.76 * 2 - r +
      PrintedNumber(hour.at(6)) + PrintedNumber(hour.at(5)) * 0.5) / 60.0;
  EXPECT_NEAR(ho, referenceHo, 0.2 / 60.0);
  EXPECT_NEAR(60 * (ho - reduction.computedAltitude),
              60 * (referenceHo - referenceHc), 0.7);
}

TEST(AlmanacAccuracy, LunarTableSemidiameterIsGeocentricAndIndependentOfObserver) {
  wxInitializer initializer;
  ASSERT_TRUE(initializer.IsOk());
  AlmanacKernelScope kernel;
  const auto instant = UtcDateTime::ToInstant(Fields("2026-08-16T12:00:00"));
  const auto near = CelestialEphemeris::Evaluate("Moon", instant, 0, 0);
  const auto far = CelestialEphemeris::Evaluate("Moon", instant, 0, 180);
  ASSERT_TRUE(near.usedDe440 && far.usedDe440);
  EXPECT_NEAR(near.geocentricSemidiameter, far.geocentricSemidiameter, 1e-12);
  EXPECT_GT(std::fabs(near.semidiameter - far.semidiameter), 0.0001);
  auto request = Request(1);
  AlmanacGenerator::ApplyPreset(AlmanacPreset::CompactAstronavigation, &request);
  const auto document = AlmanacGenerator::Build(request);
  bool found = false;
  for (const auto& page : document.pages) {
    if (page.section != "Daily ephemeris" || page.tables.empty() ||
        page.tables[0].headings[1] != "Aries GHA") continue;
    const auto& moon = page.tables.at(1);
    EXPECT_NEAR(PrintedNumber(moon.rows.at(12).at(6)),
                near.geocentricSemidiameter * 60, 0.050001);
    found = true;
  }
  EXPECT_TRUE(found);
}

TEST(AlmanacDocument, CompactPresetPreservesHourlyDataAndAvoidsDirectTableExplosion) {
  wxInitializer initializer;
  ASSERT_TRUE(initializer.IsOk());
  AlmanacRequest request = Request(2);
  AlmanacGenerator::ApplyPreset(AlmanacPreset::CompactAstronavigation, &request);
  wxString error;
  ASSERT_TRUE(AlmanacGenerator::Validate(&request, &error)) << error;
  EXPECT_FALSE(request.includeDirectReductionTables);
  EXPECT_TRUE(request.monthlyStarData);
  const auto document = AlmanacGenerator::Build(request);
  EXPECT_EQ(document.pages.size(), AlmanacGenerator::Estimate(request).pages.size());
  EXPECT_LT(document.pages.size(), 150u);
  unsigned hourly = 0, increments = 0, ageton = 0;
  for (const auto& page : document.pages) {
    EXPECT_NE(page.section, "Daily planning");
    if (page.section == "Daily ephemeris") {
      ++hourly;
      for (const auto& table : page.tables) EXPECT_EQ(table.rows.size(), 24u);
    }
    if (page.section == "Increments and corrections") ++increments;
    if (page.section == "Compact reduction tables") ++ageton;
  }
  EXPECT_EQ(hourly, 4u);
  EXPECT_EQ(increments, 60u);
  EXPECT_EQ(ageton, 46u);
  wxString output;
  if (wxGetEnv("CELNAV_COMPACT_ALMANAC_PDF", &output) && !output.empty())
    ASSERT_TRUE(AlmanacPdfWriter::Write(document, request, output, &error)) << error;
  request.toUtc = UtcDateTime::AddSeconds(request.fromUtc, 364 * 86400.0);
  EXPECT_LT(AlmanacGenerator::Estimate(request).pages.size(), 1000u);
}

TEST(AlmanacDocument, SourcesReportActualKernelAndFallbackAndManualDut1) {
  wxInitializer initializer;
  ASSERT_TRUE(initializer.IsOk());
  wxString previous;
  const bool hadPrevious = wxGetEnv("CELNAV_TEST_DE440_ENABLE", &previous);
  struct Restore {
    bool had; wxString value;
    ~Restore() { if (had) wxSetEnv("CELNAV_TEST_DE440_ENABLE", value);
                 else wxUnsetEnv("CELNAV_TEST_DE440_ENABLE"); }
  } restore{hadPrevious, previous};
  AlmanacRequest request = Request(1);
  AlmanacGenerator::ApplyPreset(AlmanacPreset::CompactAstronavigation, &request);
  request.safety = AlmanacSafety::CalculatorComplete;
  request.includeIncrementTables = request.includeCompactReductionTables = false;
  request.includeAltitudeCorrectionTables = false;
  request.dut1Known = true;
  request.dut1Seconds = 0.123;
  for (bool kernel : {false, true}) {
    if (kernel) wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
    else wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
    const auto document = AlmanacGenerator::Build(request);
    const auto& page = document.pages.at(2);
    ASSERT_EQ(page.section, "Sources and conventions");
    EXPECT_NE(page.paragraphs.at(0).Find("+0.123"), wxNOT_FOUND);
    std::map<wxString, wxString> sources;
    for (const auto& row : page.tables.at(0).rows) sources[row[0]] = row[1];
    EXPECT_EQ(sources["Sun"], kernel ? "DE440s" : "Analytical");
    EXPECT_EQ(sources["Moon"], kernel ? "DE440s" : "Analytical");
    EXPECT_EQ(sources["Venus"], kernel ? "DE440s" : "Analytical");
    EXPECT_EQ(sources["Mars"], "Analytical");
    EXPECT_EQ(sources["Jupiter"], "Analytical");
    EXPECT_EQ(sources["Saturn"], "Analytical");
  }
  request.fromUtc = request.toUtc = Fields("2200-01-01T00:00:00");
  request.dut1Known = false;
  const auto outside = AlmanacGenerator::Build(request);
  EXPECT_FALSE(outside.warnings.empty());
  for (const auto& row : outside.pages.at(2).tables.at(0).rows)
    if (row[0] == "Sun" || row[0] == "Moon") EXPECT_EQ(row[1], "Analytical");
}

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

TEST(AlmanacDependencies, AcceptsAnnualAndRejectsBackwardOrOverAnnualRanges) {
  AlmanacRequest request = Request(1);
  wxString error;
  request.toUtc = Fields("2026-08-15T00:00:00");
  EXPECT_FALSE(AlmanacGenerator::Validate(&request, &error));
  request.toUtc = Fields("2026-11-30T00:00:00");
  EXPECT_TRUE(AlmanacGenerator::Validate(&request, &error));
  request.toUtc = Fields("2027-08-17T00:00:00");
  EXPECT_FALSE(AlmanacGenerator::Validate(&request, &error));
}

TEST(AlmanacDependencies, CalculatorFreeRestoresEveryPaperDependency) {
  AlmanacRequest request = Request();
  request.safety = AlmanacSafety::CalculatorFree;
  request.selfContained = false;
  request.includeIncrementTables = false;
  request.includeCompactReductionTables = false;
  request.includeAltitudeCorrectionTables = false;
  wxString error;
  ASSERT_TRUE(AlmanacGenerator::Validate(&request, &error));
  EXPECT_TRUE(request.selfContained);
  EXPECT_TRUE(request.includeIncrementTables);
  EXPECT_TRUE(request.includeCompactReductionTables);
  EXPECT_TRUE(request.includeAltitudeCorrectionTables);
  EXPECT_NE(wxNOT_FOUND,
            AlmanacGenerator::DependencyManifest(request).Find("CALCULATOR-FREE"));
}

TEST(AlmanacAccuracy, AgetonPrintedGridTracksDirectSphericalReduction) {
  const double pi = 3.14159265358979323846;
  const double latitudes[] = {-60, -30, 15, 45, 70};
  const double declinations[] = {-23, -5, 18, 55};
  const double lhas[] = {15, 45, 80, 120, 165, 210, 280, 330};
  for (double latitude : latitudes) {
    for (double declination : declinations) {
      for (double lha : lhas) {
        const AgetonReductionResult reduced =
            AlmanacPaperTables::ReduceAgeton(latitude, declination, lha, true);
        ASSERT_TRUE(reduced.valid);
        const double rlat = latitude * pi / 180.0;
        const double rdec = declination * pi / 180.0;
        const double rlha = lha * pi / 180.0;
        const double direct = std::asin(std::sin(rlat) * std::sin(rdec) +
            std::cos(rlat) * std::cos(rdec) * std::cos(rlha)) * 180.0 / pi;
        EXPECT_NEAR(direct, reduced.computedAltitude, 0.08)
            << latitude << " " << declination << " " << lha;
        const double y = std::sin(rlha);
        const double x = std::cos(rlha) * std::sin(rlat) -
                         std::tan(rdec) * std::cos(rlat);
        double azimuth = std::atan2(y, x) * 180.0 / pi + 180.0;
        while (azimuth < 0) azimuth += 360;
        while (azimuth >= 360) azimuth -= 360;
        double azimuthDifference = std::fabs(azimuth - reduced.azimuthTrue);
        if (azimuthDifference > 180) azimuthDifference = 360 - azimuthDifference;
        EXPECT_LT(azimuthDifference, 0.35)
            << latitude << " " << declination << " " << lha;
      }
    }
  }
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

TEST(AlmanacDocument, CalculatorFreePageEstimateIncludesPaperTablesExactly) {
  AlmanacRequest request = Request(1);
  AlmanacGenerator::ApplyPreset(AlmanacPreset::CalculatorFreeVoyage, &request);
  request.fromUtc = Fields("2026-08-16T00:00:00");
  request.toUtc = request.fromUtc;
  request.coverage = AlmanacCoverage::FixedPosition;
  request.latitude = 53.19;
  request.longitude = -2.89;
  request.includeDirectReductionTables = false;
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  EXPECT_EQ(document.pages.size(), AlmanacGenerator::Estimate(request).pages.size());
  EXPECT_NE(std::find_if(document.pages.begin(), document.pages.end(),
      [](const AlmanacPage& page) { return page.section == "Increments and corrections"; }),
      document.pages.end());
  EXPECT_NE(std::find_if(document.pages.begin(), document.pages.end(),
      [](const AlmanacPage& page) { return page.section == "Compact reduction tables"; }),
      document.pages.end());
  EXPECT_NE(wxNOT_FOUND, AlmanacGenerator::DependencyManifest(request).Find(
      "CALCULATOR-FREE"));
  wxString output;
  if (wxGetEnv("CELESTIAL_CALCULATOR_FREE_TEST_PDF", &output) &&
      !output.empty()) {
    wxString error;
    ASSERT_TRUE(AlmanacPdfWriter::Write(document, request, output, &error))
        << error;
  }
  if (wxGetEnv("CELESTIAL_DIRECT_TEST_PDF", &output) && !output.empty()) {
    request.includeDirectReductionTables = true;
    request.includeStars = false;
    request.includePlanets = false;
    const AlmanacDocument directDocument = AlmanacGenerator::Build(request);
    wxString error;
    ASSERT_TRUE(AlmanacPdfWriter::Write(directDocument, request, output, &error))
        << error;
  }
}

TEST(AlmanacDocument, DirectTableScopeCanRangeFromVoyageToFullGlobal) {
  AlmanacRequest request = Request(1);
  request.includeSun = true;
  request.includeMoon = request.includePlanets = request.includeStars = false;
  request.includeDirectReductionTables = true;
  request.coverage = AlmanacCoverage::FixedPosition;
  request.latitude = 0;
  request.fullDirectReductionCoverage = false;
  EXPECT_EQ(15u, AlmanacPaperTables::DirectReductionPageCount(request));

  request.coverage = AlmanacCoverage::Global;
  request.fullDirectReductionCoverage = true;
  EXPECT_EQ(179u * 36u * 3u,
            AlmanacPaperTables::DirectReductionPageCount(request));
}

TEST(AlmanacDocument, FullGlobalAnnualEstimateMakesExtremeScaleExplicit) {
  AlmanacRequest request;
  AlmanacGenerator::ApplyPreset(AlmanacPreset::FullGlobalAlmanac, &request);
  request.fromUtc = Fields("2028-01-01T00:00:00");
  request.toUtc = Fields("2028-12-31T00:00:00");
  wxString error;
  ASSERT_TRUE(AlmanacGenerator::Validate(&request, &error));
  const AlmanacDocument estimate = AlmanacGenerator::Estimate(request);
  EXPECT_GT(estimate.pages.size(), 19000u);
  EXPECT_GT(estimate.estimatedBytes, 500u * 1024u * 1024u);
  ASSERT_FALSE(estimate.warnings.empty());
  EXPECT_NE(wxNOT_FOUND, estimate.warnings.front().Find("Very large"));
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

TEST(AlmanacPdf, BookletImposesLogicalPagesIntoSignatures) {
  AlmanacRequest request = Request(2);
  request.booklet = true;
  request.signaturePages = 16;
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  wxString output;
  const bool keep = wxGetEnv("CELESTIAL_BOOKLET_TEST_PDF", &output) &&
                    !output.empty();
  if (!keep) output = wxFileName::CreateTempFileName("celestial-booklet-");
  wxString error;
  ASSERT_TRUE(AlmanacPdfWriter::Write(document, request, output, &error)) << error;
  wxFile file(output);
  wxString bytes;
  ASSERT_TRUE(file.ReadAll(&bytes, wxConvISO8859_1));
  EXPECT_NE(wxNOT_FOUND, bytes.Find(wxString::Format(
      "/Count %u", document.physicalPdfPages)));
  EXPECT_LT(document.physicalPdfPages, document.pages.size());
  EXPECT_EQ((document.pages.size() + 3) / 4, document.sheets);
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

TEST(AlmanacPerformance, ReportsCompactAnnualAssemblyTime) {
  AlmanacRequest request = Request(366);
  request.planningIntervalDays = 30;
  request.includeDirectReductionTables = false;
  const auto started = std::chrono::steady_clock::now();
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  RecordProperty("days", 366);
  RecordProperty("pages", static_cast<int>(document.pages.size()));
  RecordProperty("assembly_ms", static_cast<int>(elapsed.count()));
  EXPECT_EQ(document.pages.size(), AlmanacGenerator::Estimate(request).pages.size());
  EXPECT_LT(elapsed.count(), 120000);
}
