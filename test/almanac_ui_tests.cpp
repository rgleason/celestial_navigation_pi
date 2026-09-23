#include <gtest/gtest.h>
#include "AlmanacDialog.h"
#include <wx/app.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/evtloop.h>
#include <wx/timer.h>
#include <wx/log.h>
#include <cstdlib>
#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {
wxChoice* Choice(wxWindow* root, const wxString& item) {
  for (auto* child : root->GetChildren()) {
    if (auto* choice = dynamic_cast<wxChoice*>(child))
      if (choice->FindString(item) != wxNOT_FOUND) return choice;
    if (auto* choice = Choice(child, item)) return choice;
  }
  return nullptr;
}
wxCheckBox* Check(wxWindow* root, const wxString& label) {
  for (auto* child : root->GetChildren()) {
    if (auto* check = dynamic_cast<wxCheckBox*>(child))
      if (check->GetLabel() == label) return check;
    if (auto* check = Check(child, label)) return check;
  }
  return nullptr;
}
void Select(wxChoice* choice, const wxString& item) {
  ASSERT_NE(choice, nullptr);
  ASSERT_TRUE(choice->SetStringSelection(item));
  wxCommandEvent event(wxEVT_CHOICE, choice->GetId());
  event.SetEventObject(choice);
  choice->ProcessWindowEvent(event);
}
void Pump() {
  wxEventLoop loop;
  wxTimer timer;
  timer.Bind(wxEVT_TIMER, [&](wxTimerEvent&) { loop.Exit(); });
  timer.StartOnce(150);
  loop.Run();
}
void Bounds(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (!child->IsShown()) continue;
    // GTK notebook page coordinates include the native notebook border.
    // Check controls within the page, not the native page/frame relationship.
    if (!dynamic_cast<wxNotebook*>(root)) {
      EXPECT_GE(child->GetPosition().x, 0) << child->GetLabel();
      EXPECT_LE(child->GetRect().GetRight(), root->GetClientSize().x + 1)
          << child->GetClassInfo()->GetClassName() << ": " << child->GetLabel();
    }
    Bounds(child);
  }
}
}

TEST(AlmanacUi, SourceControlsFitSmallAndLargeDialogs) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS")) GTEST_SKIP() << "Run alone with a display";
  int argc = 1;
  char name[] = "almanac-ui-test";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  delete wxLog::SetActiveTarget(new wxLogStderr);
  {
    AlmanacDialog dialog(nullptr);
    dialog.Show();
    auto* override = Check(&dialog, "Use one DUT1 value for the whole document");
    ASSERT_NE(override, nullptr);
    EXPECT_FALSE(override->GetValue());
    for (const auto size : {wxSize(1080, 760), wxSize(760, 560)}) {
      dialog.SetSize(size);
      for (int page = 0; page < 3; ++page) {
        dialog.SelectIntegrationPage(page);
        dialog.Layout();
        Pump();
        Bounds(&dialog);
        wxScrolledWindow* scrolling = nullptr;
        for (auto* child : dialog.GetChildren())
          if (auto* notebook = dynamic_cast<wxNotebook*>(child))
            scrolling = dynamic_cast<wxScrolledWindow*>(notebook->GetCurrentPage());
        ASSERT_NE(scrolling, nullptr);
        for (int end = 0; end < 2; ++end) {
        scrolling->Scroll(0, end ? 10000 : 0);
        Pump();
        Bounds(scrolling);
#ifdef __WXGTK3__
        auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            dialog.GetSize().x, dialog.GetSize().y);
        auto* cr = cairo_create(surface);
        gtk_widget_draw(GTK_WIDGET(dialog.GetHandle()), cr);
        EXPECT_EQ(cairo_surface_write_to_png(surface,
            wxString::Format("/tmp/celnav-2810-almanac-ui-%d-%d-%d.png", size.x, page, end).utf8_str()),
            CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
#endif
        }
      }
    }
  }
  wxTheApp->OnExit();
  wxEntryCleanup();
}
