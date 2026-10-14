#include <gtest/gtest.h>
#include <wx/wx.h>
#include <wx/choice.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/modalhook.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/srchctrl.h>
#include <cstdlib>
#include <functional>
#include "BodyCatalog.h"
#include "CelestialNavigationDialog.h"
#include "CompactEphemerisProvider.h"
#include "PlannerBodySelectionDialog.h"
#include "PlannerDialog.h"
#include "mock_plugin_api.h"
#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {
template<class T> T* Named(wxWindow* root, const wxString& name) {
  for (auto* child : root->GetChildren()) {
    if (auto* control = dynamic_cast<T*>(child))
      if (control->GetName() == name || control->GetLabel() == name) return control;
    if (auto* found = Named<T>(child, name)) return found;
  }
  return nullptr;
}
void Command(wxWindow* control, wxEventType type) {
  wxCommandEvent event(type, control->GetId());
  event.SetEventObject(control);
  control->ProcessWindowEvent(event);
}
void Choice(wxChoice* control, int value) {
  control->SetSelection(value); Command(control, wxEVT_CHOICE);
}
void Spin(wxSpinCtrlDouble* control, double value) {
  control->SetValue(value);
  wxSpinDoubleEvent event(wxEVT_SPINCTRLDOUBLE, control->GetId());
  event.SetEventObject(control); control->ProcessWindowEvent(event);
}
bool Wait(const std::function<bool()>& ready) {
  for (int i = 0; i < 1500; ++i) {
    wxTheApp->Yield(true);
    if (ready()) return true;
    wxMilliSleep(20);
  }
  return false;
}
void Capture(wxWindow* window, const char* name) {
  window->Layout();
  for (int i = 0; i < 5; ++i) { wxTheApp->Yield(true); wxMilliSleep(20); }
#ifdef __WXGTK3__
  const auto* directory = std::getenv("CELESTIAL_UI_CAPTURE_DIR");
  if (!directory) return;
  const auto size = window->GetClientSize();
  GtkAllocation allocation{0, 0, size.x, size.y};
  gtk_widget_size_allocate(GTK_WIDGET(window->GetHandle()), &allocation);
  auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
  auto* cr = cairo_create(surface);
  gtk_widget_draw(GTK_WIDGET(window->GetHandle()), cr);
  EXPECT_EQ(CAIRO_STATUS_SUCCESS, cairo_surface_write_to_png(surface,
      (wxString::FromUTF8(directory) + "/" + name + ".png").utf8_str()));
  cairo_destroy(cr); cairo_surface_destroy(surface);
#endif
}
class EditBodies : public wxModalDialogHook {
public:
  std::vector<wxString> selection;
  int answer = wxID_OK;
  bool visited = false;
  int Enter(wxDialog* dialog) override {
    auto* picker = dynamic_cast<PlannerBodySelectionDialog*>(dialog);
    if (!picker) return wxID_NONE;
    visited = true;
    Command(Named<wxButton>(picker, "Clear all"), wxEVT_BUTTON);
    for (const auto& name : selection) {
      auto* checkbox = Named<wxCheckBox>(picker, "PlannerBodyCheck_" + name);
      EXPECT_NE(nullptr, checkbox);
      if (checkbox) { checkbox->SetValue(true); Command(checkbox, wxEVT_CHECKBOX); }
    }
    auto* search = Named<wxSearchCtrl>(picker, "PlannerBodySearch");
    EXPECT_NE(nullptr, search);
    search->SetValue("Jupiter");
    EXPECT_EQ(selection.size(), picker->Selection().size());
    search->SetValue("");
    picker->Show(); Capture(picker, "custom-bodies"); picker->Hide();
    return answer;
  }
};
} // namespace

TEST(PlannerUi, VisibleBestSightsSortingCustomBodiesAndSavedChoices) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Run this wxWidgets UI test alone in its own process";
  int argc = 1; char name[] = "celnav-planner-test"; char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  const auto oldAssert = wxSetAssertHandler([](const wxString& file, int line,
      const wxString&, const wxString& condition, const wxString& message) {
    ADD_FAILURE() << file << ":" << line << " " << condition << " " << message;
  });
  delete wxLog::SetActiveTarget(new wxLogStderr);
  wxInitAllImageHandlers();
  const wxString state = wxFileName::CreateTempFileName("celnav-planner-");
  ASSERT_TRUE(wxRemoveFile(state));
  ASSERT_TRUE(wxFileName::Mkdir(state + "/plugins/celestial_navigation", 0700, wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(state);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  auto* config = GetOCPNConfigObject();
  config->SetPath("/");
  config->DeleteGroup("/PlugIns/CelestialNavigation/Planner");
  config->Write("/PlugIns/CelestialNavigation/ShowTimeIntegrity", false);
  config->Write("/PlugIns/CelestialNavigation/Planner/PositionSource", 0L);
  config->Write("/PlugIns/CelestialNavigation/Planner/Latitude", 43.95);
  config->Write("/PlugIns/CelestialNavigation/Planner/Longitude", -78.15);
  // Old Show All preferences must become alphabetical display with active fixes.
  config->Write("/PlugIns/CelestialNavigation/Planner/PlanningMode", 3L);
  celestial_navigation::SetCompactEphemerisEnabled(true);
  {
    wxFrame frame(nullptr, wxID_ANY, "Planner test host");
    celestial_navigation_pi plugin(nullptr);
    CelestialNavigationDialog main(&frame, &plugin);
    main.Show();
    auto context = [](PlannerDialog& planner, const wxString& time, long rows) {
      auto* source = Named<wxChoice>(&planner, "PlannerTimeSource");
      ASSERT_NE(nullptr, source);
      if (source) Choice(source, source->FindString("Manual date/time"));
      auto* date = Named<wxTextCtrl>(&planner, "PlannerDate");
      auto* clock = Named<wxTextCtrl>(&planner, "PlannerTime");
      ASSERT_NE(nullptr, date); ASSERT_NE(nullptr, clock);
      if (date) date->SetValue("2026-10-10");
      if (clock) clock->SetValue(time);
      auto* best = Named<wxListCtrl>(&planner, "PlannerBestSights");
      EXPECT_TRUE(Wait([&] { return best->GetItemCount() == rows; }));
    };
    {
      PlannerDialog planner(&main);
      planner.Show(); planner.SelectPageForIntegration(1);
      planner.SetSize(wxSize(1370, 950));
      auto* mode = Named<wxChoice>(&planner, "PlannerRecommendationMode");
      auto* order = Named<wxChoice>(&planner, "PlannerBodyOrder");
      auto* best = Named<wxListCtrl>(&planner, "PlannerBestSights");
      auto* bodies = Named<wxListCtrl>(&planner, "PlannerBodies");
      auto* summary = Named<wxStaticText>(&planner, "PlannerRecommendationSummary");
      auto* maximum = Named<wxSpinCtrlDouble>(&planner, "PlannerMaxHc");
      auto* minimum = Named<wxSpinCtrlDouble>(&planner, "PlannerMinHc");
      ASSERT_NE(nullptr, mode); ASSERT_NE(nullptr, order); ASSERT_NE(nullptr, best);
      ASSERT_NE(nullptr, bodies); ASSERT_NE(nullptr, summary);
      ASSERT_NE(nullptr, maximum); ASSERT_NE(nullptr, minimum);
      EXPECT_EQ(0, mode->GetSelection()); EXPECT_EQ(1, order->GetSelection());
      EXPECT_EQ(wxNOT_FOUND, mode->FindString("Show All"));
      context(planner, "13:05:16", 4);
      EXPECT_TRUE(best->IsShownOnScreen()); EXPECT_TRUE(bodies->IsShownOnScreen());
      EXPECT_TRUE(planner.GetScreenRect().Contains(best->GetScreenRect()));
      EXPECT_TRUE(planner.GetScreenRect().Contains(bodies->GetScreenRect()));
      Capture(&planner, "best-sights-and-bodies");
      Choice(order, 0);
      EXPECT_EQ(3, bodies->FindItem(-1, "Moon"));
      auto* settings = Named<wxButton>(&planner, "Fix settings...");
      ASSERT_NE(nullptr, settings);
      EXPECT_FALSE(maximum->IsShownOnScreen());
      Command(settings, wxEVT_BUTTON);
      EXPECT_TRUE(maximum->IsShownOnScreen());
      Capture(&planner, "expanded-fix-settings");
      Choice(order, 6); EXPECT_EQ(4, best->GetItemCount());
      Choice(order, 1); EXPECT_EQ(4, best->GetItemCount());
      Spin(maximum, 44); EXPECT_EQ(0, best->GetItemCount());
      EXPECT_TRUE(summary->GetLabel().Contains("at least two"));
      EXPECT_TRUE(summary->GetLabel().Contains("outside Hc 10.0 to 44.0"));
      Capture(&planner, "empty-results-explained");
      Command(settings, wxEVT_BUTTON);
      EXPECT_FALSE(maximum->IsShownOnScreen());
      Spin(minimum, 75);
      EXPECT_TRUE(summary->GetLabel().Contains("minimum Hc below the maximum"));
      Spin(minimum, 10); Spin(maximum, 75);
      context(planner, "11:05:16", 10);
      context(planner, "10:05:16", 10);
      context(planner, "13:05:16", 4);
      auto* choose = Named<wxButton>(&planner, "Choose bodies...");
      auto* bodySet = Named<wxChoice>(&planner, "PlannerBodySet");
      ASSERT_NE(nullptr, choose); ASSERT_NE(nullptr, bodySet);
      EditBodies edit; edit.Register();
      edit.selection = {"Sun"}; edit.answer = wxID_CANCEL;
      Choice(bodySet, 1); EXPECT_EQ(0, bodySet->GetSelection());
      EXPECT_EQ(4, best->GetItemCount());
      edit.selection = {"Sun", "Moon", "Jupiter", "Mars"}; edit.answer = wxID_OK;
      Command(choose, wxEVT_BUTTON);
      EXPECT_TRUE(edit.visited); EXPECT_EQ(1, bodySet->GetSelection());
      EXPECT_EQ(4, bodies->GetItemCount()); EXPECT_EQ(4, best->GetItemCount());
      const long moon = bodies->FindItem(-1, "Moon");
      ASSERT_NE(-1, moon); EXPECT_EQ(wxColour(150,150,150), bodies->GetItemTextColour(moon));
      Capture(&planner, "custom-four-bodies");
      edit.selection = {"Capella"}; Command(choose, wxEVT_BUTTON);
      Choice(mode, 2);
      auto* lunar = Named<wxListCtrl>(&planner, "PlannerLunarPairs");
      ASSERT_NE(nullptr, lunar);
      EXPECT_EQ(1, lunar->GetItemCount()); EXPECT_EQ("Moon + Capella", lunar->GetItemText(0));
      EXPECT_EQ(1, bodies->GetItemCount());
      Capture(&planner, "custom-lunar-companion");
      Choice(mode, 0);
      edit.selection.clear(); Command(choose, wxEVT_BUTTON);
      EXPECT_EQ(0, bodies->GetItemCount()); EXPECT_EQ(0, best->GetItemCount());
      EXPECT_TRUE(summary->GetLabel().Contains("No bodies selected"));
      edit.Unregister();
      planner.Hide();
    }
    {
      PlannerDialog reopened(&main);
      EXPECT_EQ(1, Named<wxChoice>(&reopened, "PlannerBodySet")->GetSelection());
      reopened.Show(); reopened.SelectPageForIntegration(1);
      context(reopened, "13:05:16", 0);
      EXPECT_TRUE(Wait([&] { return Named<wxStaticText>(&reopened,
          "PlannerRecommendationSummary")->GetLabel().Contains("No bodies selected"); }));
      EXPECT_EQ(0, Named<wxListCtrl>(&reopened, "PlannerBodies")->GetItemCount());
      Choice(Named<wxChoice>(&reopened, "PlannerBodySet"), 0);
      EXPECT_EQ(4, Named<wxListCtrl>(&reopened, "PlannerBestSights")->GetItemCount());
      EXPECT_GT(Named<wxListCtrl>(&reopened, "PlannerBodies")->GetItemCount(), 4);
      reopened.Hide();
    }
    main.Hide();
  }
  celestial_navigation::SetCompactEphemerisEnabled(false);
  config->SetPath("/"); config->DeleteGroup("/PlugIns/CelestialNavigation/Planner");
  wxSetAssertHandler(oldAssert);
  wxTheApp->OnExit(); wxEntryCleanup();
}
