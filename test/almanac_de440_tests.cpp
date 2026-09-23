#include <gtest/gtest.h>
#include "AlmanacGenerator.h"
#include "AlmanacPaperTables.h"
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"
#include <wx/init.h>
#include <wx/utils.h>
#include <map>
#include <cmath>
namespace {
wxDateTime Fields(const char* text) { wxDateTime date; EXPECT_TRUE(date.ParseISOCombined(text)); return date; }
AlmanacRequest Request(unsigned = 1) { AlmanacRequest r; r.fromUtc=r.toUtc=Fields("2026-08-16T00:00:00"); return r; }
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
  AlmanacGenerator::ApplyPreset(AlmanacPreset::CalculatorFreeVoyage, &request);
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
  AlmanacGenerator::ApplyPreset(AlmanacPreset::CalculatorFreeVoyage, &request);
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
  AlmanacGenerator::ApplyPreset(AlmanacPreset::CalculatorFreeVoyage, &request);
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
