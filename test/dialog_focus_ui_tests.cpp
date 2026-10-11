#include <gtest/gtest.h>
#include <wx/wx.h>
#include <wx/display.h>
#include <wx/filename.h>
#include <wx/modalhook.h>
#include <cstdlib>
#include <functional>
#include <memory>

#include "CelestialNavigationDialog.h"
#include "DialogFocus.h"
#include "FindBodyDialog.h"
#include "SightDialog.h"
#include "mock_plugin_api.h"

#ifndef __OCPN__ANDROID__
namespace {
class ModalAction : public wxModalDialogHook {
public:
  std::function<void(wxDialog*)> action;
  int Enter(wxDialog* dialog) override {
    wxTheApp->CallAfter([this, dialog] {
      auto callback = action;
      callback(dialog);
      if (dialog->IsModal()) dialog->EndModal(wxID_CANCEL);
    });
    return wxID_NONE;
  }
};

wxTextCtrl* Latitude(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (auto* text = dynamic_cast<wxTextCtrl*>(child))
      if (text->GetParent()->GetLabel() == "Latitude") return text;
    if (auto* text = Latitude(child)) return text;
  }
  return nullptr;
}

void Activate(wxEvtHandler* handler, wxEventType type, bool active = true) {
  wxActivateEvent event(type, active);
  event.SetEventObject(handler);
  handler->ProcessEvent(event);
}

void DrainRecovery() { wxTheApp->ProcessPendingEvents(); }

void ExpectOnDisplay(wxDialog* dialog) {
  const int display = wxDisplay::GetFromWindow(dialog);
  ASSERT_NE(wxNOT_FOUND, display);
  EXPECT_TRUE(wxDisplay(display).GetClientArea().Contains(dialog->GetRect()));
}
}  // namespace

TEST(DialogFocusUi, RecoversNestedFindWithoutChangingEditsOrModalTransactions) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Run alone with CELESTIAL_RUN_UI_TESTS=1 and a display";
  int argc = 1;
  char name[] = "celestial-dialog-focus-test";
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
  const wxString state =
      wxFileName::CreateTempFileName("celestial-focus-test-");
  ASSERT_TRUE(wxRemoveFile(state));
  ASSERT_TRUE(wxFileName::Mkdir(state + "/plugins/celestial_navigation", 0777,
                                wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(state);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  wxInitAllImageHandlers();
  {
    wxFrame frame(nullptr, wxID_ANY, "OpenCPN dialog focus test");
    frame.Show();
    SetTestCanvasWindow(&frame);
    celestial_navigation_pi plugin(nullptr);
    plugin.Init();
    plugin.OnToolbarToolCallback(0);
    CelestialNavigationDialog* main = nullptr;
    for (auto* child : frame.GetChildren())
      if ((main = dynamic_cast<CelestialNavigationDialog*>(child))) break;
    ASSERT_NE(nullptr, main);
    EXPECT_TRUE(main->IsShown());
    wxDateTime time;
    ASSERT_TRUE(time.ParseISOCombined("2024-06-13T19:26:00"));
    Sight sight(Sight::ALTITUDE, "Sun", Sight::LOWER, time, 7200, 51.5, 0.2);
    sight.m_DRLat = 41.0;
    sight.m_DRLon = -71.0;
    sight.Recompute(0);
    const Sight original = sight;
    ModalAction hook;
    int findVisits = 0;
    hook.action = [&](wxDialog* dialog) {
      if (auto* properties = dynamic_cast<SightDialog*>(dialog)) {
        EXPECT_TRUE(properties->IsModal());
        wxCommandEvent find;
        properties->OnFindBody(find);
        EXPECT_EQ(1, findVisits);
        EXPECT_TRUE(properties->IsModal());
        EXPECT_TRUE(properties->IsEnabled());
        // Once Find closes, a toolbar click must recover its modal parent.
        properties->Move(wxPoint(-10000, -10000));
        plugin.OnToolbarToolCallback(0);
        ExpectOnDisplay(properties);
        EXPECT_FALSE(main->BringToFront());
        properties->EndModal(wxID_CANCEL);
      } else if (auto* finder = dynamic_cast<FindBodyDialog*>(dialog)) {
        ++findVisits;
        EXPECT_TRUE(finder->IsModal());
        EXPECT_TRUE(finder->IsEnabled());
        EXPECT_FALSE(main->BringToFront());
        auto* latitude = Latitude(finder);
        ASSERT_NE(nullptr, latitude);
        latitude->SetValue("42 N");
        latitude->SetFocus();
        latitude->SetSelection(0, 2);
        finder->Move(wxPoint(-10000, -10000));
        main->Raise();
        plugin.OnToolbarToolCallback(0);
        DrainRecovery();
        ExpectOnDisplay(finder);
        EXPECT_EQ("42 N", latitude->GetValue());
        long from, to;
        latitude->GetSelection(&from, &to);
        EXPECT_EQ(0, from);
        EXPECT_EQ(2, to);

        // Returning to OpenCPN or activating an ancestor recovers Find,
        // while activating a sibling tool does not redirect attention.
        finder->Move(wxPoint(-10000, -10000));
        wxDialog sibling(main, wxID_ANY, "Sibling tool");
        Activate(&sibling, wxEVT_ACTIVATE);
        DrainRecovery();
        EXPECT_EQ(wxPoint(-10000, -10000), finder->GetPosition());
        Activate(&frame, wxEVT_ACTIVATE);
        DrainRecovery();
        ExpectOnDisplay(finder);
        finder->Move(wxPoint(-10000, -10000));
        Activate(wxTheApp, wxEVT_ACTIVATE_APP);
        DrainRecovery();
        ExpectOnDisplay(finder);
        finder->Move(wxPoint(-10000, -10000));
        Activate(wxTheApp, wxEVT_ACTIVATE_APP);
        Activate(wxTheApp, wxEVT_ACTIVATE_APP, false);
        DrainRecovery();
        EXPECT_EQ(wxPoint(-10000, -10000), finder->GetPosition());

        // An unrelated modal window must retain attention, even if the
        // plugin toolbar callback is invoked programmatically.
        auto outerAction = hook.action;
        hook.action = [&](wxDialog* other) {
          // Native window managers may reposition the owner when another
          // modal is shown. Recovery itself must leave that position alone.
          const wxPoint positionBeforeRecovery = finder->GetPosition();
          EXPECT_TRUE(finder->IsModal());
          EXPECT_FALSE(main->BringToFront());
          plugin.OnToolbarToolCallback(0);
          Activate(wxTheApp, wxEVT_ACTIVATE_APP);
          DrainRecovery();
          EXPECT_EQ(positionBeforeRecovery, finder->GetPosition());
          other->EndModal(wxID_CANCEL);
        };
        wxDialog unrelated(&frame, wxID_ANY, "Other plugin modal");
        unrelated.ShowModal();
        hook.action = outerAction;
        // Queue recovery and close before it runs: no retained Find pointer.
        Activate(wxTheApp, wxEVT_ACTIVATE_APP);
        finder->EndModal(wxID_CANCEL);
      } else {
        ADD_FAILURE() << "Unexpected modal dialog";
      }
    };
    hook.Register();
    {
      SightDialog properties(main, sight, 0);
      EXPECT_EQ(wxID_CANCEL, properties.ShowModal());
    }
    DrainRecovery();
    EXPECT_DOUBLE_EQ(original.m_DRLat, sight.m_DRLat);
    EXPECT_DOUBLE_EQ(original.m_DRLon, sight.m_DRLon);
    EXPECT_DOUBLE_EQ(original.m_Measurement, sight.m_Measurement);
    EXPECT_TRUE(main->IsEnabled());
    main->Hide();
    plugin.OnToolbarToolCallback(0);
    EXPECT_TRUE(main->IsShown());

    // Pending recovery is cancelled when its controller is destroyed.
    wxDialog temporaryRoot(&frame, wxID_ANY, "Temporary plugin root");
    auto controller = std::make_unique<DialogFocusController>(&temporaryRoot);
    hook.action = [&](wxDialog* child) {
      child->Move(wxPoint(-10000, -10000));
      Activate(wxTheApp, wxEVT_ACTIVATE_APP);
      controller.reset();
      DrainRecovery();
      EXPECT_EQ(wxPoint(-10000, -10000), child->GetPosition());
      child->EndModal(wxID_CANCEL);
    };
    wxDialog temporaryChild(&temporaryRoot, wxID_ANY, "Temporary modal");
    temporaryChild.ShowModal();
    hook.Unregister();
    // Exercise synchronous plugin shutdown and then drain the event queue.
    plugin.DeInit();
    DrainRecovery();
    SetTestCanvasWindow(nullptr);
  }
  SetTestPrivateDataPath(wxString());
  SetTestPluginDataRoot(wxString());
  wxSetAssertHandler(oldAssert);
  wxTheApp->OnExit();
  wxEntryCleanup();
}
#endif
