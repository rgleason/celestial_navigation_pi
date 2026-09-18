#include <gtest/gtest.h>

#include <wx/app.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filename.h>
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
      if (choice->GetCount() == 2 &&
          choice->GetString(1) == "Each sight's DR Shift")
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
    FixDialog fix(&main);
    fix.Update(0);
    fix.Show();
    fix.Layout();
    auto* motion = MotionChoice(&fix);
    ASSERT_NE(nullptr, motion);
    EXPECT_TRUE(motion->IsShown());
    EXPECT_EQ(1, motion->GetSelection());
    EXPECT_TRUE(Find<wxCheckBox>(
                    &fix, "Propagate every sight to a common epoch")
                    ->GetValue());
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
    auto* unshiftedMode = MotionChoice(&unshifted);
    ASSERT_NE(nullptr, unshiftedMode);
    EXPECT_EQ(0, unshiftedMode->GetSelection());
    EXPECT_FALSE(Find<wxCheckBox>(
        &unshifted, "Propagate every sight to a common epoch")->GetValue());
    main.m_Sights.front().m_ShiftNm = 1.4;
    main.m_Sights.front().SetVisible(false);
    FixDialog hiddenShift(&main);
    hiddenShift.Update(0);
    auto* hiddenMode = MotionChoice(&hiddenShift);
    ASSERT_NE(nullptr, hiddenMode);
    EXPECT_EQ(0, hiddenMode->GetSelection());
  }
  SetTestPrivateDataPath(wxString());
  SetTestPluginDataRoot(wxString());
  wxSetAssertHandler(oldAssert);
  wxTheApp->OnExit();
  wxEntryCleanup();
}
