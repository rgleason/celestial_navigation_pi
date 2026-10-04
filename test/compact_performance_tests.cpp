#include <gtest/gtest.h>
#include "CompactEphemerisProvider.h"
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"
#include "celestial_navigation_pi.h"
#include "CelestialNavigationDialog.h"
#include "PlannerDialog.h"
#include "NavigationUIUtils.h"
#include "mock_plugin_api.h"
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/listctrl.h>
#include <wx/stattext.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/fileconf.h>
#include <wx/utils.h>
#include <chrono>
#include <fstream>
#include <sstream>

namespace {
std::vector<std::string> Fields(const std::string& line) {
  std::vector<std::string> fields;
  std::istringstream stream(line);
  std::string field;
  while (std::getline(stream, field, '\t')) fields.push_back(field);
  return fields;
}
std::vector<std::vector<std::string>> References(const std::string& kind) {
  std::ifstream input(std::string(TESTDATA) + "/compact-issue365-before.tsv");
  std::vector<std::vector<std::string>> result;
  std::string line;
  while (std::getline(input, line)) {
    const auto row = Fields(line);
    if (!row.empty() && row[0] == kind) result.push_back(row);
  }
  return result;
}
wxDateTime Instant(const std::string& iso) {
  wxDateTime fields;
  fields.ParseISOCombined(wxString::FromUTF8(iso));
  return UtcDateTime::ToInstant(fields);
}
struct CompactScope {
  wxFileConfig* config = GetOCPNConfigObject();
  bool old = false;
  wxString de;
  bool hadDe = wxGetEnv("CELNAV_TEST_DE440_ENABLE", &de);
  CompactScope() {
    config->Read("/PlugIns/CelestialNavigation/UseCompactEphemeris", &old, false);
    celestial_navigation::SetCompactEphemerisEnabled(true);
    wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
  }
  ~CompactScope() {
    celestial_navigation::SetCompactEphemerisEnabled(old);
    if (hadDe) wxSetEnv("CELNAV_TEST_DE440_ENABLE", de);
    else wxUnsetEnv("CELNAV_TEST_DE440_ENABLE");
  }
};
void ExpectSame(const celnav::Result& a, const celnav::Result& b) {
  EXPECT_DOUBLE_EQ(a.gha_deg, b.gha_deg);
  EXPECT_DOUBLE_EQ(a.declination_deg, b.declination_deg);
  EXPECT_DOUBLE_EQ(a.distance_km, b.distance_km);
  EXPECT_DOUBLE_EQ(a.airless_altitude_deg, b.airless_altitude_deg);
  EXPECT_DOUBLE_EQ(a.azimuth_deg, b.azimuth_deg);
  EXPECT_DOUBLE_EQ(a.geometric_hc_deg, b.geometric_hc_deg);
  EXPECT_DOUBLE_EQ(a.observer_semidiameter_deg, b.observer_semidiameter_deg);
  EXPECT_DOUBLE_EQ(a.epoch.dut1_seconds, b.epoch.dut1_seconds);
  EXPECT_EQ(a.epoch.dut1_available, b.epoch.dut1_available);
  EXPECT_EQ(a.epoch.warnings, b.epoch.warnings);
}
}

TEST(CompactPerformance, FrozenPreOptimizationCoordinatesAreUnchanged) {
  celnav::Options options;
  options.data_directory = CELNAV_COMPACT_TEST_DATA;
  celnav::Engine engine(options);
  const auto rows = References("STATE");
  ASSERT_EQ(1584u, rows.size());
  for (const auto& row : rows) {
    SCOPED_TRACE(row[1] + " " + row[2] + " latitude " + row[3]);
    celnav::Request request;
    request.body = row[1]; request.utc = row[2];
    request.latitude_deg = std::stod(row[3]);
    request.longitude_deg = -179.75; request.height_m = 3.7;
    request.venus_phase = request.body == "Venus";
    const auto actual = engine.Evaluate(request);
    const double values[] = {actual.gha_deg, actual.declination_deg,
        actual.distance_km, actual.airless_altitude_deg, actual.azimuth_deg,
        actual.geometric_hc_deg, actual.observer_semidiameter_deg};
    // Cross-platform libm rounding is allowed; tolerances remain far below
    // the engine's independently qualified astronomical accuracy budgets.
    for (unsigned i = 0; i < 7; ++i)
      EXPECT_NEAR(std::stod(row[i+4]), values[i], i == 2 ? 1e-6 : 1e-9);
  }
}

TEST(CompactPerformance, PhaseAndHorizonConventionsMatchPreviousEngine) {
  CompactScope scope;
  const auto phases = References("PHASE"), events = References("EVENT");
  ASSERT_EQ(12u, phases.size()); ASSERT_EQ(36u, events.size());
  for (size_t i = 0; i < phases.size(); i += 4) {
    const auto instant = Instant(phases[i][1]);
    const auto actual = NextPrincipalMoonPhases(instant, 41, -71);
    ASSERT_EQ(4u, actual.size());
    for (unsigned j = 0; j < 4; ++j) {
      EXPECT_EQ(wxString::FromUTF8(phases[i+j][2]), actual[j].name);
      wxDateTime expected = Instant(phases[i+j][3].substr(0, 19));
      expected += wxTimeSpan::Milliseconds(std::stoi(phases[i+j][3].substr(20)));
      EXPECT_LE(std::abs((actual[j].utc - expected).GetMilliseconds().ToDouble()), 1000.0);
    }
  }
  for (size_t i = 0; i < events.size(); i += 12) {
    ObserverMotion motion;
    motion.referenceUtc = Instant(events[i][1]); motion.latitude = 41; motion.longitude = -71;
    const auto actual = HorizonEventCalculator::Calculate(motion.referenceUtc, motion, 3.7);
    ASSERT_EQ(12u, actual.events.size());
    for (unsigned j = 0; j < 12; ++j) {
      EXPECT_EQ(std::stoi(events[i+j][2]), int(actual.events[j].kind));
      EXPECT_EQ(wxString::FromUTF8(events[i+j][3]),
                UtcDateTime::FormatInstant(actual.events[j].utc, "%Y-%m-%dT%H:%M:%S.%l"));
      EXPECT_NEAR(std::stod(events[i+j][4]), actual.events[j].bearingTrue, 1e-8);
    }
  }
}

TEST(CompactPerformance, SharedLunarHarmonicsMatchOriginalSummationAcrossCoverage) {
  double maximumKm = 0;
  for (int fit : {0, 1}) {
    celnav::Options options;
    options.data_directory = CELNAV_COMPACT_TEST_DATA;
    options.lunar_fit = fit;
    celnav::Engine optimized(options);
    options.reuse_lunar_arguments = false;
    celnav::Engine reference(options);
    for (int year = 1972; year <= 2100; ++year) {
      for (int month = 1; month <= 12; ++month) {
        for (int day : {1, 15}) {
          celnav::Request request;
          std::ostringstream utc;
          utc << year << '-' << (month < 10 ? "0" : "") << month
              << '-' << (day < 10 ? "0" : "") << day << "T12:17:35.125";
          request.utc = utc.str();
          const double jd = optimized.ResolveEpoch(request).tdb_jd;
          const auto expected = reference.MoonEcliptic(jd);
          const auto actual = optimized.MoonEcliptic(jd);
          SCOPED_TRACE(request.utc + " fit " + std::to_string(fit));
          for (unsigned i = 0; i < 3; ++i) {
            maximumKm = std::max(maximumKm, std::abs(expected[i] - actual[i]));
            EXPECT_NEAR(expected[i], actual[i], 1e-7); // 0.1 mm, rounding only.
          }
        }
      }
    }
  }
  std::cout << "Lunar harmonic reuse: maximum coordinate rounding difference "
            << maximumKm * 1e6 << " mm over 6192 epochs/both published fits\n";
}

TEST(CompactPerformance, CacheKeysRetainObserverAndEarthRotationInputs) {
  celnav::Options options;
  options.data_directory = CELNAV_COMPACT_TEST_DATA;
  celnav::Engine cached(options);
  options.cache_epoch_context = false;
  celnav::Engine uncached(options);
  celnav::Request base;
  base.utc = "2025-07-16T12:00:00.125";
  base.latitude_deg = 41; base.longitude_deg = -71;
  for (const auto* body : {"Sun", "Moon", "Venus", "Sirius"}) {
    base.body = body;
    for (int change = 0; change < 10; ++change) {
      auto r = base;
      switch (change) {
        case 1: r.latitude_deg = -41; break;
        case 2: r.longitude_deg = 179.75; break;
        case 3: r.height_m = 1000; break;
        case 4: r.dut1_seconds = 0.75; break;
        case 5: r.tai_minus_utc = 38; break;
        case 6: r.polar_x_arcsec = 0.3; break;
        case 7: r.polar_y_arcsec = -0.2; break;
        case 8: r.venus_phase = true; break;
        case 9: r.observer_direction = false; break;
      }
      SCOPED_TRACE(std::string(body) + " change " + std::to_string(change));
      const auto expected = uncached.Evaluate(r);
      ExpectSame(expected, cached.Evaluate(r));
      ExpectSame(expected, cached.Evaluate(r));
    }
  }
  auto invalid = base;
  invalid.dut1_seconds = INFINITY;
  EXPECT_THROW(cached.Evaluate(invalid), std::invalid_argument);
  invalid = base; invalid.utc = "not a calendar";
  EXPECT_THROW(cached.Evaluate(invalid), std::invalid_argument);
}

TEST(CompactPerformance, RepeatedExactRequestsAvoidRecomputingTheModel) {
  celnav::Options options;
  options.data_directory = CELNAV_COMPACT_TEST_DATA;
  celnav::Engine cached(options);
  options.cache_epoch_context = false;
  celnav::Engine uncached(options);
  celnav::Request request;
  request.body = "Moon"; request.utc = "2025-07-16T12:00:00.125";
  request.latitude_deg = 41; request.longitude_deg = -71;
  const auto expected = cached.Evaluate(request);
  auto measure = [&](const celnav::Engine& engine) {
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) ExpectSame(expected, engine.Evaluate(request));
    return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  };
  const double repeated = measure(cached), recomputed = measure(uncached);
  std::cout << "Exact request reuse: " << repeated << "s cached / " << recomputed << "s uncached\n";
  // Relative timings tolerate slower CI machines; a missing/ineffective cache
  // fails even when numerical comparisons still pass.
  EXPECT_LT(repeated, recomputed / 3);
}

#ifndef __OCPN__ANDROID__
TEST(CompactPerformanceUi, DialogReturnsImmediatelyAndOnlyPublishesLatestContext) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS")) GTEST_SKIP();
  int argc = 1; char name[] = "compact-planner-performance";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  delete wxLog::SetActiveTarget(new wxLogStderr);
  wxInitAllImageHandlers();
  const auto previousHandler = wxSetAssertHandler(
      [](const wxString&, int line, const wxString&, const wxString& condition,
         const wxString& message) {
        ADD_FAILURE() << line << " " << condition.ToStdString() << " " << message.ToStdString();
      });
  struct Cleanup {
    wxAssertHandler_t handler;
    ~Cleanup() {
      SetTestPrivateDataPath(wxEmptyString);
      SetTestPluginDataRoot(wxEmptyString);
      wxSetAssertHandler(handler);
      wxEntryCleanup();
    }
  } cleanup{previousHandler};
  CompactScope scope;
  wxString privatePath = wxFileName::CreateTempFileName("celnav-compact-ui-");
  wxRemoveFile(privatePath);
  ASSERT_TRUE(wxFileName::Mkdir(privatePath + "/plugins/celestial_navigation", 0700, wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(privatePath);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  scope.config->Write("/PlugIns/CelestialNavigation/ShowTimeIntegrity", false);
  scope.config->Write("/PlugIns/CelestialNavigation/Planner/PositionSource", 0L);
  scope.config->Write("/PlugIns/CelestialNavigation/Planner/EntryFormat", 0L);
  scope.config->Write("/PlugIns/CelestialNavigation/Planner/InputTimeBasis", 0L);
  scope.config->Write("/PlugIns/CelestialNavigation/Planner/ShowMoonPath", false);
  wxFrame frame(nullptr, wxID_ANY, "Compact planner performance");
  celestial_navigation_pi plugin(nullptr);
  CelestialNavigationDialog main(&frame, &plugin);
  auto begin = std::chrono::steady_clock::now();
  PlannerDialog planner(&main);
  const auto construct = std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
  EXPECT_LT(construct, 2.0); // The previous Compact dialog blocked for ~17 seconds.
  auto* status = dynamic_cast<wxStaticText*>(planner.FindWindowByName("PlannerStatus"));
  auto* latitude = dynamic_cast<wxTextCtrl*>(planner.FindWindowByName("PlannerLatitude"));
  auto* longitude = dynamic_cast<wxTextCtrl*>(planner.FindWindowByName("PlannerLongitude"));
  auto* date = dynamic_cast<wxTextCtrl*>(planner.FindWindowByName("PlannerDate"));
  auto* clock = dynamic_cast<wxTextCtrl*>(planner.FindWindowByName("PlannerTime"));
  auto* almanac = dynamic_cast<wxListCtrl*>(planner.FindWindowByName("PlannerAlmanac"));
  ASSERT_TRUE(status && latitude && longitude && date && clock && almanac);
  EXPECT_TRUE(status->GetLabel().Contains("Calculating"));
  EXPECT_EQ(0, almanac->GetItemCount());
  planner.Show();
  // Supersede the opening job and several intermediate edits. None may
  // publish old rows while the new context is waiting for its debounce.
  date->SetValue("2025-07-16"); clock->SetValue("12:00:00");
  latitude->SetValue("41.1"); latitude->SetValue("-23"); latitude->SetValue("52.5");
  longitude->SetValue("17");
  EXPECT_EQ(0, almanac->GetItemCount());
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  unsigned yields = 0;
  while (almanac->GetItemCount() == 0 && std::chrono::steady_clock::now() < deadline) {
    wxTheApp->Yield(); ++yields; wxMilliSleep(5);
  }
  ASSERT_EQ(175, almanac->GetItemCount());
  EXPECT_GT(yields, 10u); // Main-thread events continue while the worker calculates.
  const auto utc = Instant("2025-07-16T12:00:00");
  const auto expected = CelestialEphemeris::Evaluate("Sun", utc, 52.5, 17);
  ASSERT_TRUE(expected.usedCompact);
  EXPECT_EQ("2025-07-16 12:00", almanac->GetItemText(0, 0));
  EXPECT_EQ("Sun", almanac->GetItemText(0, 1));
  EXPECT_EQ(FormatNavigationAngle(expected.geometricAltitude), almanac->GetItemText(0, 7));
  EXPECT_EQ(wxString::Format("%.1f", expected.azimuthTrue), almanac->GetItemText(0, 8));
  EXPECT_TRUE(status->GetLabel().Contains("updated"));
  // Close immediately with a replacement calculation outstanding: owned
  // cancellation must join without publishing into destroyed widgets.
  latitude->SetValue("10");
  for (int i = 0; i < 75; ++i) { wxTheApp->Yield(); wxMilliSleep(5); }
  planner.Hide();
}
#endif
