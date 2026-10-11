#include <gtest/gtest.h>

#include <wx/app.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/filename.h>
#include <wx/fileconf.h>
#include <wx/frame.h>
#include <wx/log.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include <cmath>
#include <cstdlib>
#include <tuple>

#include "CelestialNavigationDialog.h"
#include "FixDialog.h"
#include "NavigationUIUtils.h"
#include <wx/combobox.h>
#include "celestial_navigation_pi.h"
#include "mock_plugin_api.h"

#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {
template <class T>
T* Find(wxWindow* root, const wxString& label) {
  for (auto* child : root->GetChildren()) {
    if (auto* value = dynamic_cast<T*>(child))
      if (value->GetLabel() == label) return value;
    if (auto* value = Find<T>(child, label)) return value;
  }
  return nullptr;
}

wxChoice* MotionChoice(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (auto* choice = dynamic_cast<wxChoice*>(child))
      if (choice->GetCount() == 3 &&
          choice->GetString(2) == "Each sight's DR Shift")
        return choice;
    if (auto* choice = MotionChoice(child)) return choice;
  }
  return nullptr;
}

wxListCtrl* ShiftResults(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (auto* list = dynamic_cast<wxListCtrl*>(child)) {
      wxListItem column;
      column.SetMask(wxLIST_MASK_TEXT);
      if (list->GetColumnCount() > 4 && list->GetColumn(4, column) &&
          column.GetText() == "DR shift NM")
        return list;
    }
    if (auto* list = ShiftResults(child)) return list;
  }
  return nullptr;
}

void CheckMotionControlsDisabled(wxWindow* root, int* count) {
  for (auto* child : root->GetChildren()) {
    if (auto* spin = dynamic_cast<wxSpinCtrlDouble*>(child)) {
      EXPECT_FALSE(spin->IsEnabled());
      ++*count;
    }
    CheckMotionControlsDisabled(child, count);
  }
}

#ifdef __WXGTK3__
void CaptureFix(wxWindow& window, const char* path) {
  window.Show(); window.Layout(); wxTheApp->Yield(true);
  const auto size = window.GetSize();
  GtkAllocation allocation{0, 0, size.x, size.y};
  gtk_widget_size_allocate(GTK_WIDGET(window.GetHandle()), &allocation);
  auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
  auto* cr = cairo_create(surface);
  gtk_widget_draw(GTK_WIDGET(window.GetHandle()), cr);
  EXPECT_EQ(cairo_surface_write_to_png(surface, path), CAIRO_STATUS_SUCCESS);
  cairo_destroy(cr); cairo_surface_destroy(surface);
}
#endif
}  // namespace

TEST(FixUi, SavedVisibleDrShiftsSelectRunningFixAndShowInputs) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Run alone with CELESTIAL_RUN_UI_TESTS=1 and a display";
  int argc = 1;
  char name[] = "celestial-fix-ui-test";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
#ifdef __WXGTK3__
  if (std::getenv("CELESTIAL_FIX_LARGE_FONT"))
    g_object_set(gtk_settings_get_default(), "gtk-font-name", "Sans 15", nullptr);
#endif
  const auto oldAssert = wxSetAssertHandler(
      [](const wxString& file, int line, const wxString&,
         const wxString& condition, const wxString& message) {
        ADD_FAILURE() << file << ":" << line << " " << condition << " "
                      << message;
      });
  delete wxLog::SetActiveTarget(new wxLogStderr);
  const wxString state = wxFileName::CreateTempFileName("celestial-fix-test-");
  ASSERT_TRUE(wxRemoveFile(state));
  ASSERT_TRUE(wxFileName::Mkdir(state + "/plugins/celestial_navigation", 0777,
                                wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(state);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  const wxString magneticModel =
      wxFileName(__FILE__).GetPath() + "/../data/IGRF11.COF";
  ASSERT_EQ(0, geomag_load(magneticModel.mb_str()));
  wxInitAllImageHandlers();
  {
    wxFrame frame(nullptr, wxID_ANY, "Fix test host");
    celestial_navigation_pi plugin(nullptr);
    PlugIn_Position_Fix_Ex boat{};
    boat.Lat = 42.0;
    boat.Lon = -70.0;
    plugin.SetPositionFixEx(boat);
    CelestialNavigationDialog main(&frame, &plugin);
    wxDateTime first, second, third;
    ASSERT_TRUE(first.ParseISOCombined("2026-09-18T11:00:00"));
    ASSERT_TRUE(second.ParseISOCombined("2026-09-18T11:30:00"));
    ASSERT_TRUE(third.ParseISOCombined("2026-09-18T12:00:00"));
    for (const auto& entry : {
             std::make_tuple("Sun", first, 1.4, 45.0),
             std::make_tuple("Moon", second, 0.7, 125.0),
             std::make_tuple("Venus", third, 0.0, 0.0)}) {
      Sight sight(Sight::ALTITUDE, std::get<0>(entry), Sight::CENTER,
                  std::get<1>(entry), 0, 40.0, 0.2);
      sight.m_DRLat = 42.0;
      sight.m_DRLon = -70.0;
      sight.m_ShiftNm = std::get<2>(entry);
      sight.m_ShiftBearing = std::get<3>(entry);
      sight.m_bMagneticShiftBearing = false;
      sight.Recompute(0);
      main.m_Sights.push_back(sight);
    }
    main.m_Sights[1].m_bMagneticShiftBearing = true;
    auto* config = GetOCPNConfigObject();
    config->SetPath("/PlugIns/CelestialNavigation/DialogGeometry");
    config->Write("FixWidth", 1030L);
    config->Write("FixHeight", 1000L);
    config->DeleteEntry("FixDesktopWidth");
    config->DeleteEntry("FixDesktopHeight");
    FixDialog fix(&main);
    EXPECT_NE(wxSize(1030, 1000), fix.GetSize());
    fix.Update(0);
    fix.Show();
    fix.Layout();
    auto* motion = MotionChoice(&fix);
    ASSERT_NE(nullptr, motion);
    EXPECT_TRUE(motion->IsShown());
    EXPECT_EQ(2, motion->GetSelection());
    auto* results = ShiftResults(&fix);
    ASSERT_NE(nullptr, results);
    ASSERT_EQ(3, results->GetItemCount());
    EXPECT_EQ("1.40", results->GetItemText(0, 4));
    EXPECT_EQ(wxString::Format("45.0%c T", 0x00b0),
              results->GetItemText(0, 5));
    EXPECT_EQ(wxString::Format("45.0%c T", 0x00b0),
              results->GetItemText(0, 6));
    EXPECT_EQ("0.70", results->GetItemText(1, 4));
    EXPECT_EQ(wxString::Format("125.0%c M", 0x00b0),
              results->GetItemText(1, 5));
    const wxString usedBearing = results->GetItemText(1, 6);
    EXPECT_TRUE(usedBearing.EndsWith(" T"));
    double usedDegrees = NAN;
    EXPECT_TRUE(usedBearing.BeforeFirst(wxChar(0x00b0)).ToDouble(&usedDegrees));
    EXPECT_TRUE(std::isfinite(usedDegrees));
    EXPECT_TRUE(results->GetItemText(2, 5).empty());
    int disabledMotionControls = 0;
    CheckMotionControlsDisabled(&fix, &disabledMotionControls);
    EXPECT_EQ(2, disabledMotionControls);
    main.m_Sights[1].m_bMagneticShiftBearing = false;
    fix.Update(0);
    EXPECT_TRUE(std::isfinite(fix.m_fixlat));
    EXPECT_TRUE(std::isfinite(fix.m_fixlon));
    wxTheApp->Yield(true);
#ifdef __WXGTK3__
    const auto size = fix.GetSize();
    GtkAllocation allocation{0, 0, size.x, size.y};
    gtk_widget_size_allocate(GTK_WIDGET(fix.GetHandle()), &allocation);
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                size.x, size.y);
    auto* cr = cairo_create(surface);
    gtk_widget_draw(GTK_WIDGET(fix.GetHandle()), cr);
    EXPECT_EQ(cairo_surface_write_to_png(
                  surface, "/tmp/celestial-fix-2861.png"),
              CAIRO_STATUS_SUCCESS);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
#endif
    fix.Hide();
    for (Sight& sight : main.m_Sights) sight.m_ShiftNm = 0.0;
    FixDialog unshifted(&main);
    unshifted.Update(0);
    unshifted.Show(); unshifted.Layout();
    auto* unshiftedMode = MotionChoice(&unshifted);
    ASSERT_NE(nullptr, unshiftedMode);
    EXPECT_EQ(0, unshiftedMode->GetSelection());
    main.m_Sights.front().m_ShiftNm = 1.4;
    main.m_Sights.front().SetVisible(false);
    FixDialog hiddenShift(&main);
    hiddenShift.Update(0);
    auto* hiddenMode = MotionChoice(&hiddenShift);
    ASSERT_NE(nullptr, hiddenMode);
    EXPECT_EQ(0, hiddenMode->GetSelection());

    auto* source = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("FixDrSource", &unshifted));
    auto* go = Find<wxButton>(&unshifted, "Show fix on chart");
    ASSERT_NE(nullptr, source); ASSERT_NE(nullptr, go);
    EXPECT_NE(nullptr, Find<wxButton>(&unshifted, "Calculate"));
    EXPECT_GE(source->GetSelection(), 2);
    EXPECT_GE(unshifted.GetSize().x, unshifted.GetSizer()->GetMinSize().x);
    EXPECT_EQ(nullptr, wxWindow::FindWindowByName("FixCandidate0", &unshifted));
    auto* algorithm = dynamic_cast<wxComboBox*>(wxWindow::FindWindowByName("FixAlgorithm", &unshifted));
    ASSERT_NE(nullptr, algorithm); EXPECT_TRUE(algorithm->IsEnabled());
    auto setChoice = [](wxChoice* choice, int value) {
      choice->SetSelection(value);
      wxCommandEvent command(wxEVT_CHOICE, choice->GetId()); command.SetEventObject(choice);
      choice->GetEventHandler()->ProcessEvent(command);
    };
    setChoice(unshiftedMode, 1);
    EXPECT_FALSE(algorithm->IsEnabled());
    auto* course = wxWindow::FindWindowByName("FixCourse", &unshifted);
    ASSERT_NE(nullptr, course); EXPECT_TRUE(course->GetParent()->IsShown());
    setChoice(unshiftedMode, 0); EXPECT_TRUE(algorithm->IsEnabled());
    EXPECT_FALSE(course->GetParent()->IsShown());

    // Bob's observations still calculate through the ordinary desktop path
    // once the visible inline DR is set to his estimate. No candidate gate.
    CelestialNavigationDialog bob(&frame, &plugin);
    boat.Lat = 64.23; boat.Lon = -41.48; plugin.SetPositionFixEx(boat);
    bob.m_Sights.clear();
    wxDateTime capellaTime, schedarTime;
    ASSERT_TRUE(capellaTime.ParseISOCombined("2026-07-20T22:39:25"));
    ASSERT_TRUE(schedarTime.ParseISOCombined("2026-07-20T22:48:48"));
    for (const auto& entry : {std::make_tuple("Capella", capellaTime, 23+38.8/60),
                             std::make_tuple("Schedar", schedarTime, 38.0)}) {
      Sight sight(Sight::ALTITUDE, std::get<0>(entry), Sight::CENTER,
                  std::get<1>(entry), 0, std::get<2>(entry), 0.2);
      sight.m_DRLat = 7+20.0/60; sight.m_DRLon = 100+40.0/60;
      sight.m_IndexError = 112.8; sight.m_EyeHeight = 3.7;
      sight.m_Temperature = 10; sight.m_Pressure = 1010;
      sight.Recompute(0); bob.m_Sights.push_back(sight);
    }
    FixDialog bobFix(&bob);
    auto* bobGo = Find<wxButton>(&bobFix, "Show fix on chart");
    auto* bobSource = dynamic_cast<wxChoice*>(wxWindow::FindWindowByName("FixDrSource", &bobFix));
    auto* lat = dynamic_cast<NavigationAngleCtrl*>(wxWindow::FindWindowByName("FixDrLatitude", &bobFix));
    auto* lon = dynamic_cast<NavigationAngleCtrl*>(wxWindow::FindWindowByName("FixDrLongitude", &bobFix));
    ASSERT_NE(nullptr, bobGo); ASSERT_NE(nullptr, bobSource);
    ASSERT_NE(nullptr, lat); ASSERT_NE(nullptr, lon);
    EXPECT_TRUE(bobSource->GetStringSelection().Contains("Schedar"));
    bobFix.Update(0);
    EXPECT_TRUE(bobGo->IsEnabled());
    EXPECT_NEAR(3+17.1/60, bobFix.m_fixlat, 0.003);
    EXPECT_NEAR(99+19.5/60, bobFix.m_fixlon, 0.003);
    setChoice(bobSource, 1); EXPECT_NEAR(64+1.42/60, bobFix.m_fixlat, .03);
    bobFix.Update(0); EXPECT_EQ(1, bobSource->GetSelection());
    lat->SetValue("7 20 N"); lon->SetValue("100 40 E");
    EXPECT_EQ(0, bobSource->GetSelection());
    bobFix.Update(0); EXPECT_EQ(0, bobSource->GetSelection());
    EXPECT_NEAR(3+17.1/60, bobFix.m_fixlat, .003);
    lat->SetValue("invalid");
    EXPECT_FALSE(bobGo->IsEnabled()); EXPECT_FALSE(std::isfinite(bobFix.m_fixlat));
    EXPECT_EQ("N/A", dynamic_cast<wxTextCtrl*>(wxWindow::FindWindowByName("FixLatitude", &bobFix))->GetValue());
    lat->SetValue("7 20 N"); EXPECT_TRUE(bobGo->IsEnabled());
    for (auto& sight : bob.m_Sights) { sight.m_DRLat = NAN; sight.m_DRLon = NAN; }
    bobFix.Update(0); EXPECT_TRUE(lat->GetValue().empty()); EXPECT_TRUE(lon->GetValue().empty());
    EXPECT_FALSE(bobGo->IsEnabled());
    auto* bobMotion = MotionChoice(&bobFix); setChoice(bobMotion, 1);
    EXPECT_FALSE(bobGo->IsEnabled());
    lat->SetValue("7 20 N"); lon->SetValue("100 40 E");
    bobFix.Update(0); EXPECT_TRUE(bobGo->IsEnabled());
    setChoice(bobMotion, 0);

    auto* openFixButton = Find<wxButton>(&bob, "Fix...");
    ASSERT_NE(nullptr, openFixButton);
    // The guide uses Bob's own three-star running-fix example (issue 370).
    CelestialNavigationDialog hawaii(&frame, &plugin);
    hawaii.m_Sights.clear();
    for (const auto& data : {
        std::make_tuple("Vega", "2025-07-16T06:30:15", 37+50./60, 18+51.6/60, -168-45.9/60, 1.5),
        std::make_tuple("Menkent", "2025-07-16T06:35:50", 33+28.8/60, 18+51.4/60, -168-46.6/60, .78),
        std::make_tuple("Dubhe", "2025-07-16T06:43:44", 30+39./60, 18+51.2/60, -168-47.4/60, 0.)}) {
      wxDateTime utc; ASSERT_TRUE(utc.ParseISOCombined(std::get<1>(data)));
      Sight sight(Sight::ALTITUDE, std::get<0>(data), Sight::CENTER, utc, 0, std::get<2>(data), .2);
      sight.m_DRLat=std::get<3>(data); sight.m_DRLon=std::get<4>(data);
      sight.m_ShiftNm=std::get<5>(data); sight.m_ShiftBearing=255;
      sight.m_bMagneticShiftBearing=false; sight.m_EyeHeight=3;
      sight.m_IndexError=1.5; sight.m_Temperature=25; sight.m_Pressure=1015;
      sight.Recompute(0); hawaii.m_Sights.push_back(sight);
    }
    FixDialog guideFix(&hawaii); guideFix.Update(0);
    EXPECT_NEAR(18+52.6604/60, guideFix.m_fixlat, .005);
    EXPECT_NEAR(-168-46.7496/60, guideFix.m_fixlon, .005);
#ifdef __WXGTK3__
    CaptureFix(guideFix, "/tmp/celnav297-guide-fix.png");
    hawaii.SetSize(wxSize(1200,800));
    CaptureFix(hawaii, "/tmp/celnav297-guide-menu.png");
    auto* more=Find<wxButton>(&guideFix,"More options...");
    ASSERT_NE(nullptr,more);
    wxCommandEvent expand(wxEVT_BUTTON,more->GetId()); expand.SetEventObject(more);
    more->GetEventHandler()->ProcessEvent(expand);
    CaptureFix(guideFix,"/tmp/celnav297-guide-options.png");
#endif

    wxCommandEvent openFix(wxEVT_BUTTON, openFixButton->GetId());
    openFix.SetEventObject(openFixButton);
    openFixButton->GetEventHandler()->ProcessEvent(openFix);
    ASSERT_NE(nullptr, bob.m_FixDialog);
    bob.OnFixClose();
    EXPECT_EQ(nullptr, bob.m_FixDialog);
  }
  SetTestPrivateDataPath(wxString());
  SetTestPluginDataRoot(wxString());
  wxSetAssertHandler(oldAssert);
  wxTheApp->OnExit();
  wxEntryCleanup();
}
