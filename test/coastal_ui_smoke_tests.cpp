#include <gtest/gtest.h>
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/textctrl.h>
#include <wx/modalhook.h>
#include <wx/log.h>
#include <wx/evtloop.h>
#include <wx/timer.h>
#include <cstdlib>
#include "CelestialNavigationDialog.h"
#include "CoastalNavigationDialog.h"
#include "WaypointPickerDialog.h"
#include "mock_plugin_api.h"
#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {
template <class T>
T* Find(wxWindow* root, const wxString& label) {
  for (auto* child : root->GetChildren()) {
    if (auto* match = dynamic_cast<T*>(child))
      if (match->GetLabel() == label || match->GetName() == label) return match;
    if (auto* match = Find<T>(child, label)) return match;
  }
  return nullptr;
}
void Click(wxButton* button) {
  ASSERT_NE(button, nullptr);
  wxCommandEvent event(wxEVT_BUTTON, button->GetId());
  event.SetEventObject(button);
  button->ProcessWindowEvent(event);
}
void Pump() {
  wxEventLoop loop;
  wxTimer timer;
  timer.Bind(wxEVT_TIMER, [&](wxTimerEvent&) { loop.Exit(); });
  timer.StartOnce(200);
  loop.Run();
}
void HorizontalBounds(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (!child->IsShown()) continue;
    EXPECT_GE(child->GetPosition().x, 0) << child->GetLabel();
    EXPECT_LE(child->GetRect().GetRight(), root->GetClientSize().x + 1)
        << child->GetLabel();
    HorizontalBounds(child);
  }
}
class PickerResponse : public wxModalDialogHook {
public:
  bool accept = true;
  wxString filter;
  int calls = 0;
  int Enter(wxDialog* dialog) override {
    auto* picker = dynamic_cast<WaypointPickerDialog*>(dialog);
    if (!picker) {
      ADD_FAILURE() << "Unexpected modal dialog: " << dialog->GetTitle();
      return wxID_CANCEL;
    }
    ++calls;
    if (!accept) return wxID_CANCEL;
    wxListCtrl* list = nullptr;
    for (auto* child : picker->GetChildren())
      if (auto* l = dynamic_cast<wxListCtrl*>(child)) list = l;
    EXPECT_NE(list, nullptr);
    if (!list) return wxID_CANCEL;
    if (!filter.empty()) {
      for (auto* child : picker->GetChildren())
        if (auto* search = dynamic_cast<wxSearchCtrl*>(child))
          search->SetValue(filter);
    }
    EXPECT_EQ(list->GetItemCount(), filter.empty() ? 2 : 1);
    if (!list->GetItemCount()) return wxID_CANCEL;
    list->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    wxListEvent select(wxEVT_LIST_ITEM_SELECTED, list->GetId());
    select.m_item.SetData(list->GetItemData(0));
    list->ProcessWindowEvent(select);
    EXPECT_NE(picker->GetSelectedWaypoint(), nullptr);
    return wxID_OK;
  }
};
}  // namespace

TEST(CoastalUiSmoke, LayoutAndWaypointTransactions) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Opt-in display test; run alone";
  int argc = 1;
  char name[] = "celestial-coastal-ui";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  delete wxLog::SetActiveTarget(new wxLogStderr);
  wxInitAllImageHandlers();
  const auto path = wxFileName::CreateTempFileName("coastal-ui-");
  ASSERT_TRUE(wxRemoveFile(path));
  ASSERT_TRUE(wxFileName::Mkdir(path + "/plugins/celestial_navigation", 0777,
                                wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(path);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath()+"/..");
  GetOCPNConfigObject()->Write("/PlugIns/CelestialNavigation/ShowTimeIntegrity",
                               false);
  SetTestWaypoints({{"target-1", "Needles", 50 + 39.7 / 60, -(1 + 35.5 / 60)},
                    {"target-2", "Needles", 51, -2}});
  {
    wxFrame frame(nullptr, wxID_ANY, "Coastal UI test");
    celestial_navigation_pi plugin(nullptr);
    CelestialNavigationDialog main(&frame, &plugin);
    frame.Show();
    main.Show();
    Pump();
    CoastalNavigationDialog dialog(&main);
    PickerResponse response;
    response.Register();
    ASSERT_EQ(LoadOpenCpnWaypoints().size(), 2u);
    auto field = [&](const wxString& label) -> wxTextCtrl* {
      return Find<wxTextCtrl>(&dialog, label);
    };
    auto set = [&](const wxString& label, const wxString& value) {
      auto* control = field(label);
      ASSERT_NE(control, nullptr);
      control->SetValue(value);
    };
    set("Observed vertical angle", "0 2.8");
    set("Charted top height", "24");
    set("Height of eye", "3");
    set("Index error (on the arc +)", "-0.15");
    wxChoice* mode = nullptr;
    wxNotebook* book = nullptr;
    for (auto* child : dialog.GetChildren())
      if (auto* b = dynamic_cast<wxNotebook*>(child)) book = b;
    ASSERT_NE(book, nullptr);
    for (auto* child : book->GetPage(0)->GetChildren())
      if (auto* c = dynamic_cast<wxChoice*>(child)) mode = c;
    ASSERT_NE(mode, nullptr);
    mode->SetSelection(1);
    wxCommandEvent change(wxEVT_CHOICE, mode->GetId());
    mode->ProcessWindowEvent(change);
    dialog.Show();
    Pump();
    for (const wxString label :
         {"Select target waypoint...", "Select left waypoint...",
          "Select centre waypoint...", "Select right waypoint..."}) {
      Click(Find<wxButton>(&dialog, label));
    }
    for (const wxString label :
         {"Target latitude", "Left object latitude", "Centre object latitude",
          "Right object latitude"}) {
      ASSERT_NE(field(label), nullptr);
      EXPECT_TRUE(field(label)->GetValue().Contains("39.7000"));
    }
    response.filter = "51";
    Click(Find<wxButton>(&dialog, "Select target waypoint..."));
    EXPECT_TRUE(field("Target latitude")->GetValue().StartsWith("51"));
    response.filter.clear();
    Click(Find<wxButton>(&dialog, "Select target waypoint..."));
    const wxString retained = field("Target latitude")->GetValue();
    response.accept = false;
    Click(Find<wxButton>(&dialog, "Select target waypoint..."));
    EXPECT_EQ(response.calls, 7);
    EXPECT_EQ(retained, field("Target latitude")->GetValue());
    EXPECT_EQ("24", field("Charted top height")->GetValue());
    Click(Find<wxButton>(&dialog, "Calculate and plot range"));
    EXPECT_EQ(FormatNavigationAngle(2.8 / 60),
              field("Observed vertical angle")->GetValue());
    bool found = false;
    for (auto* child : book->GetPage(0)->GetChildren())
      if (child->GetLabel().Contains("9.797 NM")) {
        found = true;
        EXPECT_TRUE(child->GetLabel().Contains("corrected angle -"));
      }
    EXPECT_TRUE(found);
    set("Left object latitude", "43 45.9N");
    set("Left object longitude", "69 19W");
    set("Centre object latitude", "43 57.9N");
    set("Centre object longitude", "69 4.4W");
    set("Right object latitude", "43 47N");
    set("Right object longitude", "68 51.3W");
    set("Left-centre measurement", "111");
    set("Centre-right measurement", "107");
    set("Approximate vessel latitude", "43 49.7N");
    set("Approximate vessel longitude", "69 4.6W");
    Find<wxTextCtrl>(book->GetPage(1), "Index error (on the arc +)")
        ->SetValue("-0.15");
    Click(Find<wxButton>(&dialog, "Solve and plot HSA fix"));
    for (int page : {0, 1})
      for (const wxSize size : {wxSize(900, 760), wxSize(720, 500)}) {
        book->SetSelection(page);
        dialog.SetSize(size);
        dialog.Layout();
        Pump();
        EXPECT_EQ(dialog.GetSize(), size);
#ifdef __WXGTK3__
        // With no host main loop, Wayland's native allocation can lag the wx
        // size request. Allocate the requested client area before inspecting
        // it.
        GtkAllocation requested{0, 0, size.x, size.y};
        gtk_widget_size_allocate(GTK_WIDGET(dialog.GetHandle()), &requested);
        wxSizeEvent resized(dialog.GetSize(), dialog.GetId());
        dialog.ProcessWindowEvent(resized);
        dialog.Layout();
#endif
        auto* scroll = dynamic_cast<wxScrolledWindow*>(book->GetPage(page));
        ASSERT_NE(scroll, nullptr);
        EXPECT_LE(scroll->GetVirtualSize().x, scroll->GetClientSize().x);
        HorizontalBounds(scroll);
        scroll->Scroll(0, 10000);
#ifdef __WXGTK3__
        gtk_widget_size_allocate(GTK_WIDGET(dialog.GetHandle()), &requested);
        auto* surface =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
        auto* cr = cairo_create(surface);
        gtk_widget_draw(GTK_WIDGET(dialog.GetHandle()), cr);
        EXPECT_EQ(cairo_surface_write_to_png(
                      surface, wxString::Format("/tmp/coastal-2853-%d-%d.png",
                                                page, size.x)
                                   .utf8_str()),
                  CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
#endif
        scroll->Scroll(0, 0);
      }
    dialog.Hide();
    dialog.Show();
    EXPECT_EQ(retained, field("Target latitude")->GetValue());
    dialog.Hide();
  }
  SetTestWaypoints({});
  SetTestPrivateDataPath(wxEmptyString);
  SetTestPluginDataRoot(wxEmptyString);
  wxFileName::Rmdir(path, wxPATH_RMDIR_RECURSIVE);
  wxTheApp->OnExit();
  wxEntryCleanup();
}
