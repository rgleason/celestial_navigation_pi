#include <gtest/gtest.h>
#include "OcpnApiCompat.h"
#include "mock_plugin_api.h"
#include "LunarPresentation.h"
#include "LunarResultsDialog.h"
#include "LunarSolutionRecord.h"
#include "CompactEphemerisProvider.h"
#include "NavigationUIUtils.h"
#include "Sight.h"
#include "UtcDateTime.h"
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/stattext.h>
#include <wx/log.h>
#include <wx/utils.h>
#include <cmath>
#include <cstdlib>
#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

TEST(LunarResultsUi,
     ReferenceEditsAndCandidateSelectionsKeepRecordedInputsAndFit) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS")) GTEST_SKIP();
  int argc = 1;
  char name[] = "celnav299-lunar-results-ui";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  const auto assertHandler = wxSetAssertHandler(
      [](const wxString& file, int line, const wxString&,
         const wxString& condition, const wxString& message) {
        ADD_FAILURE() << file.ToStdString() << ":" << line << " "
                      << condition.ToStdString() << " "
                      << message.ToStdString();
      });
  delete wxLog::SetActiveTarget(new wxLogStderr);
  const bool previousCompact = celestial_navigation::CompactEphemerisEnabled();
  celestial_navigation::SetCompactEphemerisEnabled(true);
  {
    wxFrame frame(nullptr, wxID_ANY, "Isolated lunar UI test");
    const auto epoch = UtcDateTime::Create(2026, 9, 19, 1);
    Sight sight(Sight::LUNAR, "Saturn", Sight::LUNAR_FAR, epoch, 54800,
                104 + 34.0 / 60, .2, true);
    sight.m_AllowDe440 = false;
    sight.m_LunarMoonAltitude = 13 + 8.0 / 60;
    sight.m_LunarBodyAltitude = 11 + 33.0 / 60;
    sight.m_LunarMoonLimb = Sight::LOWER;
    sight.m_LunarBodyLimb = Sight::CENTER;
    sight.m_LunarMoonAltitudeUncertainty =
        sight.m_LunarBodyAltitudeUncertainty = .5;
    sight.m_IndexError = -1.5;
    sight.m_EyeHeight = 3;
    sight.m_Temperature = 11;
    sight.m_Pressure = 1010;
    sight.m_DRLat = 43 + 16.7898 / 60;
    sight.m_DRLon = -(76 + 58.4122 / 60);
    sight.m_CorrectedDateTime = epoch;
    sight.RecomputeLunar();
    ASSERT_TRUE(sight.m_LunarSolutionValid);
    const auto snapshot = LunarInputSnapshot(sight);
    const auto retained = sight.m_LunarCalculation;
    LunarResultsDialog dialog(&frame, sight);
    dialog.Show();
    auto settle = [&]() {
      dialog.Layout();
      for (int i = 0; i < 5; ++i) {
        wxTheApp->Yield();
        wxMilliSleep(30);
      }
    };
    settle();
    auto find = [&](const char* name) {
      return wxWindow::FindWindowByName(name, &dialog);
    };
    auto* latitude = dynamic_cast<wxTextCtrl*>(find("LunarKnownLatitude"));
    auto* longitude = dynamic_cast<wxTextCtrl*>(find("LunarKnownLongitude"));
    auto* error = dynamic_cast<wxStaticText*>(find("LunarDistanceCheck"));
    auto* positions = dynamic_cast<wxListCtrl*>(find("LunarPositions"));
    auto* candidates = dynamic_cast<wxListCtrl*>(find("LunarCandidates"));
    auto* details = dynamic_cast<wxTextCtrl*>(find("LunarDetailedTrail"));
    auto* pages = dynamic_cast<wxNotebook*>(find("LunarPages"));
    auto* calculations =
        dynamic_cast<wxNotebook*>(find("LunarCalculationPages"));
    ASSERT_TRUE(latitude && longitude && error && positions && candidates &&
                details && pages && calculations);
    EXPECT_NEAR(.243, wxAtof(error->GetLabel()), .002);
    const double originalError = wxAtof(error->GetLabel());
    ASSERT_GE(positions->GetItemCount(), 3);
    std::vector<wxString> originalPositions;
    for (int row = 0; row < positions->GetItemCount(); ++row)
      originalPositions.push_back(positions->GetItemText(row, 1) +
                                  positions->GetItemText(row, 2));
    const auto originalLatitude = latitude->GetValue();
    const auto originalLongitude = longitude->GetValue();
    latitude->SetValue("invalid");
    settle();
    EXPECT_EQ("Unavailable", error->GetLabel());
    EXPECT_FALSE(details->GetValue().Contains("Lunar error (cleared - true):"));
    latitude->SetValue(originalLatitude);
    longitude->SetValue("077 30 W");
    settle();
    EXPECT_GT(std::fabs(originalError - wxAtof(error->GetLabel())), .001);
    // Longitude changes the correction at fixed geometry; it cannot change the
    // fitted positions.
    for (int row = 0; row < positions->GetItemCount(); ++row)
      EXPECT_EQ(originalPositions[row], positions->GetItemText(row, 1) +
                                            positions->GetItemText(row, 2));
    longitude->SetValue(originalLongitude);
    settle();
    EXPECT_EQ(snapshot, LunarInputSnapshot(sight));
    EXPECT_EQ(retained, sight.m_LunarCalculation);
    for (long row = 0; row < candidates->GetItemCount(); ++row) {
      candidates->SetItemState(row, wxLIST_STATE_SELECTED,
                               wxLIST_STATE_SELECTED);
      settle();
      EXPECT_EQ(row, sight.m_LunarSelectedCandidate);
      EXPECT_EQ(snapshot, LunarInputSnapshot(sight));
      EXPECT_EQ(retained, sight.m_LunarCalculation);
      EXPECT_TRUE(details->GetValue().Contains(
          lunar_presentation::SolutionId(row) + " | "));
      EXPECT_NEAR(.243, wxAtof(error->GetLabel()), .002);
    }
    const wxString captureDir =
        std::getenv("CELNAV_UI_CAPTURE_DIR")
            ? wxString::FromUTF8(std::getenv("CELNAV_UI_CAPTURE_DIR"))
            : "/tmp/celnav299-ui";
    ASSERT_TRUE(wxFileName::DirExists(captureDir) ||
                wxFileName::Mkdir(captureDir, 0777, wxPATH_MKDIR_FULL));
    auto capture = [&](const wxString& name) {
      settle();
#ifdef __WXGTK3__
      const auto size = dialog.GetSize();
      GtkAllocation allocation{0, 0, size.x, size.y};
      gtk_widget_size_allocate(GTK_WIDGET(dialog.GetHandle()), &allocation);
      auto* surface =
          cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
      auto* cr = cairo_create(surface);
      gtk_widget_draw(GTK_WIDGET(dialog.GetHandle()), cr);
      EXPECT_EQ(CAIRO_STATUS_SUCCESS,
                cairo_surface_write_to_png(
                    surface, (captureDir + "/" + name + ".png").utf8_str()));
      cairo_destroy(cr);
      cairo_surface_destroy(surface);
#endif
    };
    for (const auto size : {wxSize(1080, 820), wxSize(900, 700)}) {
      dialog.SetSize(size);
      settle();
      auto* results = dynamic_cast<wxScrolledWindow*>(pages->GetPage(0));
      ASSERT_NE(nullptr, results);
      EXPECT_LE(results->GetVirtualSize().x, results->GetClientSize().x + 2);
      capture(wxString::Format("results-%d", size.x));
    }
    dialog.SetSize(wxSize(1080, 820));
    pages->SetSelection(1);
    for (std::size_t page = 0; page < calculations->GetPageCount(); ++page) {
      calculations->SetSelection(page);
      capture(wxString::Format("calculations-%zu-top", page));
      if (auto* scroll =
              dynamic_cast<wxScrolledWindow*>(calculations->GetPage(page))) {
        EXPECT_LE(scroll->GetVirtualSize().x, scroll->GetClientSize().x + 2);
        scroll->Scroll(0, 100000);
        capture(wxString::Format("calculations-%zu-bottom", page));
        scroll->Scroll(0, 0);
      }
    }
    EXPECT_EQ(snapshot, LunarInputSnapshot(sight));
  }
  celestial_navigation::SetCompactEphemerisEnabled(previousCompact);
  wxSetAssertHandler(assertHandler);
  wxTheApp->OnExit();
  wxEntryCleanup();
}
