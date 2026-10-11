#include <gtest/gtest.h>
#include <wx/wx.h>
#include <wx/choice.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/modalhook.h>
#include <wx/spinctrl.h>
#include <cstdlib>
#include <vector>

#include "CelestialNavigationDialog.h"
#include "HorizonEventDialog.h"
#include "SightAnalysisDialog.h"
#include "mock_plugin_api.h"
#include "UtcDateTime.h"

#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {
std::vector<wxWindow*> Descendants(wxWindow* parent) {
  std::vector<wxWindow*> result;
  for (auto* child : parent->GetChildren()) {
    result.push_back(child);
    const auto nested = Descendants(child);
    result.insert(result.end(), nested.begin(), nested.end());
  }
  return result;
}

wxListCtrl* Report(wxWindow* parent, int columns) {
  for (auto* child : Descendants(parent))
    if (auto* list = dynamic_cast<wxListCtrl*>(child))
      if (list->GetColumnCount() == columns) return list;
  return nullptr;
}

void Capture(wxWindow* window, const wxString& name) {
  window->Show();
  window->Layout();
  wxTheApp->Yield(true);
#ifdef __WXGTK3__
  const char* directory = std::getenv("CELESTIAL_UI_CAPTURE_DIR");
  if (!directory) return;
  const auto size = window->GetClientSize();
  GtkAllocation allocation{0, 0, size.x, size.y};
  gtk_widget_size_allocate(GTK_WIDGET(window->GetHandle()), &allocation);
  auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                             size.x, size.y);
  auto* cr = cairo_create(surface);
  gtk_widget_draw(GTK_WIDGET(window->GetHandle()), cr);
  EXPECT_EQ(CAIRO_STATUS_SUCCESS,
            cairo_surface_write_to_png(surface,
                (wxString::FromUTF8(directory) + "/" + name + ".png").utf8_str()));
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
#endif
}

void DragColumn(wxListCtrl* list, int column, int width) {
  list->SetColumnWidth(column, width);
  wxListEvent event(wxEVT_LIST_COL_END_DRAG, list->GetId());
  event.SetEventObject(list);
  event.SetColumn(column);
  list->GetEventHandler()->ProcessEvent(event);
}

void Analyze(wxWindow* dialog) {
  for (auto* child : Descendants(dialog))
    if (auto* button = dynamic_cast<wxButton*>(child))
      if (button->GetLabel() == "Analyze visible sights") {
        wxCommandEvent event(wxEVT_BUTTON, button->GetId());
        event.SetEventObject(button);
        button->GetEventHandler()->ProcessEvent(event);
        return;
      }
  ADD_FAILURE() << "Analyze button missing";
}

class HorizonUtcCheck : public wxModalDialogHook {
public:
  bool visited = false;
  int Enter(wxDialog* dialog) override {
    if (!dynamic_cast<HorizonEventDialog*>(dialog)) return wxID_NONE;
    visited = true;
    const auto before = UtcDateTime::Fields(UtcDateTime::Now());
    wxSpinCtrl* hours = nullptr;
    for (auto* child : Descendants(dialog))
      if (auto* spin = dynamic_cast<wxSpinCtrl*>(child))
        if (spin->GetMax() == 23) hours = spin;
    EXPECT_NE(nullptr, hours);
    if (hours) {
      EXPECT_EQ(before.hour, hours->GetValue());
    }
    return wxID_CANCEL;
  }
};
}  // namespace

TEST(DesktopFormattingUi, ReadableControlsAndPersistentReportColumns) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Run alone with a display and CELESTIAL_RUN_UI_TESTS=1";
  int argc = 1;
  char name[] = "celestial-desktop-formatting-test";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  const auto oldAssert = wxSetAssertHandler(
      [](const wxString& file, int line, const wxString&,
         const wxString& condition, const wxString& message) {
        ADD_FAILURE() << file << ":" << line << " " << condition << " " << message;
      });
  delete wxLog::SetActiveTarget(new wxLogStderr);
  const wxString state = wxFileName::CreateTempFileName("celnav-formatting-");
  ASSERT_TRUE(wxRemoveFile(state));
  ASSERT_TRUE(wxFileName::Mkdir(state + "/plugins/celestial_navigation", 0777,
                                wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(state);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  GetOCPNConfigObject()->Write("/PlugIns/CelestialNavigation/ShowTimeIntegrity", false);
  wxInitAllImageHandlers();
  {
    wxFrame frame(nullptr, wxID_ANY, "Desktop formatting test host");
    wxDateTime utc;
    ASSERT_TRUE(utc.ParseISOCombined("2026-10-11T23:59:59"));
    Sight eventSight(Sight::HORIZON, "Sun", Sight::UPPER, utc, 1, 0, 1);
    eventSight.m_Pressure = 1100;
    {
      HorizonEventDialog horizon(&frame, eventSight, 0, "Test UTC source");
      Capture(&horizon, "horizon-large-font");
      for (auto* child : Descendants(&horizon)) {
        if (auto* spin = dynamic_cast<wxSpinCtrl*>(child)) {
          const int needed = spin->GetSizeFromTextSize(
              spin->GetTextExtent(wxString::Format("%d", spin->GetMax())).x).x;
          EXPECT_GE(spin->GetSize().x, needed) << "Integer " << spin->GetValue();
        } else if (auto* spin = dynamic_cast<wxSpinCtrlDouble*>(child)) {
          const wxString sample = wxString::Format("%.*f", spin->GetDigits(), spin->GetMax());
          const int needed = spin->GetSizeFromTextSize(spin->GetTextExtent(sample).x).x;
          EXPECT_GE(spin->GetSize().x, needed) << "Number " << spin->GetValue();
        } else if (auto* choice = dynamic_cast<wxChoice*>(child)) {
          int needed = 0;
          for (unsigned i = 0; i < choice->GetCount(); ++i)
            needed = std::max(needed, choice->GetTextExtent(choice->GetString(i)).x);
          EXPECT_GE(choice->GetSize().x, needed + choice->FromDIP(24));
        }
      }
      horizon.Hide();
    }
    celestial_navigation_pi plugin(nullptr);
    int customSightWidth = 0;
    {
      CelestialNavigationDialog main(&frame, &plugin);
      HorizonUtcCheck utcCheck;
      utcCheck.Register();
      for (auto* child : Descendants(&main))
        if (auto* button = dynamic_cast<wxButton*>(child))
          if (button->GetLabel() == "Horizon Event...") {
            wxCommandEvent event(wxEVT_BUTTON, button->GetId());
            event.SetEventObject(button);
            button->GetEventHandler()->ProcessEvent(event);
          }
      utcCheck.Unregister();
      EXPECT_TRUE(utcCheck.visited);
      EXPECT_TRUE(main.m_Sights.empty());
      for (const char* time : {"2026-09-01T08:00:00", "2026-09-01T09:00:00", "2026-10-11T08:00:00"}) {
        ASSERT_TRUE(utc.ParseISOCombined(time));
        Sight sight(Sight::ALTITUDE, "Sun", Sight::LOWER, utc, 1, 2, .2);
        sight.m_DRLat = 33.358;
        sight.m_DRLon = -79.282;
        sight.m_DRBoatPosition = false;
        sight.m_bCalculated = true;
        sight.m_ObservedAltitude = 1.0;
        main.m_Sights.push_back(sight);
      }
      int customWidth = 0;
      {
        SightAnalysisDialog analyzer(&main);
        Capture(&analyzer, "analysis-long-span");
        for (auto* child : Descendants(&analyzer)) {
          if (auto* spin = dynamic_cast<wxSpinCtrlDouble*>(child)) {
            EXPECT_GE(spin->GetSize().x, spin->GetSizeFromTextSize(
                spin->GetTextExtent(wxString::Format("%.1f", spin->GetMax())).x).x);
          }
        }
        auto* list = Report(&analyzer, 6);
        ASSERT_NE(nullptr, list);
        ASSERT_EQ(3, list->GetItemCount());
        for (int column = 0; column < list->GetColumnCount(); ++column)
          for (long row = 0; row < list->GetItemCount(); ++row)
            EXPECT_GE(list->GetColumnWidth(column),
                      list->GetTextExtent(list->GetItemText(row, column)).x + list->FromDIP(16));
        customWidth = list->FromDIP(360);
        DragColumn(list, 0, customWidth);
        Analyze(&analyzer);
        EXPECT_EQ(customWidth, list->GetColumnWidth(0));
        analyzer.Hide();
      }
      {
        SightAnalysisDialog reopened(&main);
        auto* list = Report(&reopened, 6);
        ASSERT_NE(nullptr, list);
        EXPECT_EQ(customWidth, list->GetColumnWidth(0));
      }
      main.UpdateSights();
      auto* list = Report(&main, 7);
      ASSERT_NE(nullptr, list);
      for (long row = 0; row < list->GetItemCount(); ++row)
        EXPECT_GE(list->GetColumnWidth(3),
                  list->GetTextExtent(list->GetItemText(row, 3)).x + list->FromDIP(16));
      customSightWidth = list->FromDIP(310);
      DragColumn(list, 4, customSightWidth);
    }
    {
      CelestialNavigationDialog reopened(&frame, &plugin);
      auto* list = Report(&reopened, 7);
      ASSERT_NE(nullptr, list);
      EXPECT_EQ(customSightWidth, list->GetColumnWidth(4));
    }
  }
  SetTestPrivateDataPath(wxString());
  SetTestPluginDataRoot(wxString());
  wxSetAssertHandler(oldAssert);
  wxTheApp->OnExit();
  wxEntryCleanup();
  wxFileName::Rmdir(state, wxPATH_RMDIR_RECURSIVE);
}
