#include <gtest/gtest.h>
#include <cstdlib>
#include <functional>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/image.h>
#include <wx/listctrl.h>
#include <wx/log.h>
#include <wx/modalhook.h>
#include <wx/spinctrl.h>

#include "CelestialNavigationDialog.h"
#include "SightDialog.h"
#include "mock_plugin_api.h"
#include "tinyxml.h"

namespace {
class Main : public CelestialNavigationDialog {
public:
  using CelestialNavigationDialog::CelestialNavigationDialog;
  using CelestialNavigationDialogBase::m_lSights;
};

wxSpinCtrl* SearchSpan(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (auto* spin = dynamic_cast<wxSpinCtrl*>(child))
      if (spin->GetMax() == 172800) return spin;
    if (auto* spin = SearchSpan(child)) return spin;
  }
  return nullptr;
}

wxButton* EditButton(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (auto* button = dynamic_cast<wxButton*>(child))
      if (button->GetLabel() == "Edit") return button;
    if (auto* button = EditButton(child)) return button;
  }
  return nullptr;
}

class EditHook : public wxModalDialogHook {
public:
  std::function<void(SightDialog*)> edit;
  int Enter(wxDialog* dialog) override {
    if (auto* properties = dynamic_cast<SightDialog*>(dialog)) {
      wxTheApp->CallAfter([this, properties] {
        edit(properties);
        if (properties->IsModal()) {
          ADD_FAILURE() << "Test did not dismiss the sight editor";
          properties->EndModal(wxID_CANCEL);
        }
      });
      return wxID_NONE;
    }
    ADD_FAILURE() << "Unexpected dialog: " << dialog->GetTitle();
    return wxID_CANCEL;
  }
};

void Edit(Main* main) {
  auto* button = EditButton(main);
  ASSERT_NE(nullptr, button);
  ASSERT_TRUE(button->IsEnabled());
  wxCommandEvent event(wxEVT_BUTTON, button->GetId());
  event.SetEventObject(button);
  button->ProcessWindowEvent(event);
}

wxString XmlContents(const wxString& path) {
  wxFFile file(path, "r");
  wxString result;
  EXPECT_TRUE(file.IsOpened());
  EXPECT_TRUE(file.ReadAll(&result));
  return result;
}

void ExpectSavedSpan(const wxString& path, double expected) {
  TiXmlDocument document;
  ASSERT_TRUE(document.LoadFile(path.mb_str())) << document.ErrorDesc();
  ASSERT_NE(nullptr, document.RootElement());
  auto* sight = document.RootElement()->FirstChildElement("Sight");
  ASSERT_NE(nullptr, sight);
  double span = -1;
  ASSERT_EQ(TIXML_SUCCESS, sight->QueryDoubleAttribute("TimeCertainty", &span));
  EXPECT_DOUBLE_EQ(expected, span);
}

Sight Fixture(Sight::Type type, double span) {
  Sight sight(type, "Saturn", Sight::LUNAR_FAR,
              wxDateTime(19, wxDateTime::Sep, 2026, 1, 0, 0),
              span, type == Sight::LUNAR ? 104 + 33.8 / 60 : 11.55, 0.2);
  sight.m_DRBoatPosition = false;
  sight.m_DRLat = 43.28;
  sight.m_DRLon = -76.97;
  sight.m_LunarMoonAltitude = 13 + 8.0 / 60;
  sight.m_LunarBodyAltitude = 11.55;
  sight.m_bVisible = false;
  return sight;
}
}  // namespace

// Run alone with CELESTIAL_RUN_UI_TESTS=1 and a display. All persistence is
// redirected to a temporary profile; the user's OpenCPN is never accessed.
TEST(SightSearchSpanUi, DefaultsEditXmlReloadAndCancelPreserveValues) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Run alone with CELESTIAL_RUN_UI_TESTS=1 and a display";
  int argc = 1;
  char name[] = "celestial-search-span-test";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  delete wxLog::SetActiveTarget(new wxLogStderr);
  wxInitAllImageHandlers();
  const wxString state = wxFileName::CreateTempFileName("celestial-span-test-");
  ASSERT_FALSE(state.empty());
  ASSERT_TRUE(wxRemoveFile(state));
  ASSERT_TRUE(wxFileName::Mkdir(state + "/plugins/celestial_navigation", 0777,
                                wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(state);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  {
    wxFrame frame(nullptr, wxID_ANY, "Search span regression");
    // A new lunar still gets the 24-hour default. Zero time uncertainty on an
    // ordinary altitude sight must not acquire that lunar default.
    for (auto type : {Sight::LUNAR, Sight::ALTITUDE}) {
      Sight sight = Fixture(type, 0);
      SightDialog dialog(&frame, sight, 0, wxDateTime(),
                         SightDialog::Mode::Create);
      ASSERT_NE(nullptr, SearchSpan(&dialog));
      EXPECT_EQ(type == Sight::LUNAR ? 86400 : 0,
                SearchSpan(&dialog)->GetValue());
      EXPECT_DOUBLE_EQ(type == Sight::LUNAR ? 86400 : 0, sight.m_TimeCertainty);
    }

    celestial_navigation_pi plugin(nullptr);
    Main main(&frame, &plugin);
    EditHook hook;
    hook.Register();
    const wxString xml = state + "/plugins/celestial_navigation/Sights.xml";
    for (auto type : {Sight::LUNAR, Sight::ALTITUDE}) {
      for (double originalSpan : {5400., 10800., 20000., 58400., 86400., 172800.}) {
        SCOPED_TRACE(::testing::Message() << "type=" << type
                     << " span=" << originalSpan);
        const Sight original = Fixture(type, originalSpan);
        main.m_Sights.assign(1, original);
        main.m_lSights->DeleteAllItems();
        main.m_lSights->InsertItem(0, "Search span test");
        main.m_lSights->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        const int editedSpan = originalSpan == 86400 ? 58400 : 86400;
        int visits = 0;
        hook.edit = [&](SightDialog* dialog) {
          ++visits;
          auto* span = SearchSpan(dialog);
          ASSERT_NE(nullptr, span);
          EXPECT_EQ(originalSpan, span->GetValue());
          EXPECT_DOUBLE_EQ(originalSpan, main.m_Sights[0].m_TimeCertainty);
          // String entry exercises typed edits as well as numeric values.
          span->SetValue(wxString::Format("%d", editedSpan));
          dialog->EndModal(wxID_OK);
        };
        Edit(&main);
        EXPECT_EQ(1, visits);
        EXPECT_DOUBLE_EQ(editedSpan, main.m_Sights[0].m_TimeCertainty);
        ExpectSavedSpan(xml, editedSpan);
        const wxString beforeCancel = XmlContents(xml);
        hook.edit = [&](SightDialog* dialog) {
          ++visits;
          auto* span = SearchSpan(dialog);
          ASSERT_NE(nullptr, span);
          EXPECT_EQ(editedSpan, span->GetValue());
          span->SetValue(999);
          dialog->Recompute();
          dialog->EndModal(wxID_CANCEL);
        };
        Edit(&main);
        EXPECT_EQ(2, visits);
        EXPECT_DOUBLE_EQ(editedSpan, main.m_Sights[0].m_TimeCertainty);
        EXPECT_EQ(beforeCancel, XmlContents(xml));
        EXPECT_DOUBLE_EQ(original.m_Measurement, main.m_Sights[0].m_Measurement);
        EXPECT_EQ(original.m_DateTime, main.m_Sights[0].m_DateTime);
        {
          Main reloaded(&frame, &plugin);
          ASSERT_EQ(1u, reloaded.m_Sights.size());
          EXPECT_DOUBLE_EQ(editedSpan, reloaded.m_Sights[0].m_TimeCertainty);
          SightDialog dialog(&frame, reloaded.m_Sights[0], 0);
          ASSERT_NE(nullptr, SearchSpan(&dialog));
          EXPECT_EQ(editedSpan, SearchSpan(&dialog)->GetValue());
        }
      }
    }
    hook.Unregister();
  }
  SetTestPrivateDataPath(wxString());
  SetTestPluginDataRoot(wxString());
  EXPECT_TRUE(wxFileName::Rmdir(state, wxPATH_RMDIR_RECURSIVE));
  wxTheApp->OnExit();
  wxEntryCleanup();
}
