#include <gtest/gtest.h>
#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/modalhook.h>
#include <wx/notebook.h>
#include <cstdlib>
#include <functional>
#include "CelestialNavigationDialog.h"
#include "CompactEphemerisProvider.h"
#include "EclipseDialog.h"
#include "EclipseDataFiles.h"
#include "LunarToolsDialog.h"
#include "eclipse/dut1.h"
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
void Click(wxWindow* control) {
  ASSERT_NE(nullptr, control);
  if (!control) return;
  wxCommandEvent event(wxEVT_BUTTON, control->GetId());
  event.SetEventObject(control); control->ProcessWindowEvent(event);
}
bool Wait(const std::function<bool()>& ready) {
  for (int i = 0; i < 1000; ++i) {
    wxTheApp->Yield(true);
    if (ready()) return true;
    wxMilliSleep(20);
  }
  return false;
}
void Capture(wxWindow* window, const char* name) {
  window->Layout(); wxTheApp->Yield(true);
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
class Dialogs : public wxModalDialogHook {
public:
  wxString file, message;
  bool entryVisited = false;
  int Enter(wxDialog* dialog) override {
    if (auto* picker = dynamic_cast<wxFileDialog*>(dialog)) {
      picker->SetPath(file);
      // GTK resolves the chooser's selection asynchronously, even when the
      // modal hook supplies the response without opening its window.
      EXPECT_TRUE(Wait([&] { return picker->GetPath() == file; })) << picker->GetPath();
      return wxID_OK;
    }
    if (auto* warning = dynamic_cast<wxMessageDialog*>(dialog)) {
      message = warning->GetMessage(); return wxID_OK;
    }
    if (dialog->GetTitle() == "Enhanced Accuracy Add-ons") {
      entryVisited = true;
      EXPECT_NE(nullptr, Named<wxStaticText>(dialog, "De440DataStatus"));
      EXPECT_NE(nullptr, Named<wxStaticText>(dialog, "Dut1DataStatus"));
      return wxID_CANCEL;
    }
    return wxID_NONE;
  }
};
}

TEST(AccuracyAddonsUi, MainEntryVerifiedImportsOfflineStatusAndExistingRoutes) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Run alone in a wxWidgets UI process";
  int argc = 1; char name[] = "celnav-addons-test"; char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv)); ASSERT_TRUE(wxTheApp->CallOnInit());
  const auto oldAssert = wxSetAssertHandler([](const wxString& file, int line,
      const wxString&, const wxString& condition, const wxString& message) {
    ADD_FAILURE() << file << ":" << line << " " << condition << " " << message;
  });
  delete wxLog::SetActiveTarget(new wxLogStderr); wxInitAllImageHandlers();
  const wxString state = wxFileName::CreateTempFileName("celnav-addons-");
  ASSERT_TRUE(wxRemoveFile(state));
  ASSERT_TRUE(wxFileName::Mkdir(state + "/plugins/celestial_navigation/eclipse",
                              0700, wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(state);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  const auto previousDut1 = eclipse::GetDut1Update();
  eclipse::SetDut1Update(nullptr);
  const bool previousCompact = celestial_navigation::CompactEphemerisEnabled();
  celestial_navigation::SetCompactEphemerisEnabled(true);
  {
    wxFrame frame(nullptr, wxID_ANY, "Accuracy add-ons host");
    celestial_navigation_pi plugin(nullptr);
    CelestialNavigationDialog main(&frame, &plugin); main.Show();
    Dialogs dialogs; dialogs.Register();
    auto* entry = Named<wxButton>(&main, "DE440s / DUT1...");
    auto* pdf = Named<wxButton>(&main, "Reference Manual (PDF)...");
    ASSERT_NE(nullptr, entry); ASSERT_NE(nullptr, pdf);
    EXPECT_GT(entry->GetPosition().y, pdf->GetPosition().y);
    const int calls = TestDownloadCalls();
    Click(entry); EXPECT_TRUE(dialogs.entryVisited);
    EXPECT_EQ(calls, TestDownloadCalls());
    Capture(&main, "accuracy-addons-main");
    EclipseDialog addons(&main, &plugin, EclipseDialog::View::AccuracyAddons);
    addons.Show();
    auto* de = Named<wxStaticText>(&addons, "De440DataStatus");
    auto* dut1 = Named<wxStaticText>(&addons, "Dut1DataStatus");
    ASSERT_NE(nullptr, de); ASSERT_NE(nullptr, dut1);
    EXPECT_TRUE(de->GetLabel().Contains("Not installed"));
    EXPECT_TRUE(dut1->GetLabel().Contains("Bundled coverage"));
    EXPECT_TRUE(dut1->GetLabel().Contains("using bundled data"));
    EXPECT_EQ(nullptr, Named<wxCheckBox>(&addons, "UseCompactEphemeris"));
    EXPECT_EQ(calls, TestDownloadCalls());
    Capture(&addons, "accuracy-addons-missing");
    Click(Named<wxButton>(&addons, "Dut1Download"));
    EXPECT_EQ(calls + 1, TestDownloadCalls());
    EXPECT_EQ(nullptr, eclipse::GetDut1Update());
    Click(Named<wxButton>(&addons, "De440Download"));
    EXPECT_GT(TestDownloadCalls(), calls + 1);
    EXPECT_TRUE(dialogs.message.Contains("trusted download sources"));
    EXPECT_FALSE(addons.IsInstallingData());
    const wxString destination = state + "/plugins/celestial_navigation/eclipse/de440s.bsp";
    dialogs.file = wxString::FromUTF8(ECLIPSE_DE440_TEST_PATH);
    ASSERT_TRUE(wxFileExists(dialogs.file));
    Click(Named<wxButton>(&addons, "De440Import"));
    ASSERT_TRUE(Wait([&] { return !addons.IsInstallingData(); }));
    EXPECT_TRUE(de->GetLabel().Contains("Installed and verified")) << dialogs.message << " status=" << de->GetLabel();
    EXPECT_FALSE(Named<wxButton>(&addons, "De440Download")->IsEnabled());
    EXPECT_TRUE(wxFileExists(destination));
    const wxString invalid = state + "/invalid.bsp";
    { wxFile file(invalid, wxFile::write); ASSERT_TRUE(file.Write("invalid")); }
    dialogs.file = invalid;
    Click(Named<wxButton>(&addons, "De440Import"));
    ASSERT_TRUE(Wait([&] { return !addons.IsInstallingData(); }));
    EXPECT_TRUE(de->GetLabel().Contains("Installed and verified"));
    EXPECT_EQ(celestial_navigation::InstalledDataState::Verified,
        celestial_navigation::InspectInstalledData(celestial_navigation::EclipseDataKind::De440s,
                                                   destination));
    Click(Named<wxButton>(&addons, "Verify installed DE440s"));
    ASSERT_TRUE(Wait([&] { return !addons.IsInstallingData(); }));
    EXPECT_TRUE(de->GetLabel().Contains("Installed and verified"));
    addons.SetSize(700, 680); Capture(&addons, "accuracy-addons-verified-small");
    addons.SetSize(1080, 960); Capture(&addons, "accuracy-addons-verified-large");
    // A damaged installed file must lose verified status, then recover after
    // a valid replacement. Checking/importing must never alter the provider setting.
    ASSERT_TRUE(wxCopyFile(invalid, destination, true));
    addons.RefreshInstalledData();
    ASSERT_TRUE(Wait([&] { return !addons.IsInstallingData(); }));
    EXPECT_TRUE(de->GetLabel().Contains("failed verification"));
    EXPECT_TRUE(Named<wxButton>(&addons, "De440Download")->IsEnabled());
    ASSERT_TRUE(wxCopyFile(wxString::FromUTF8(ECLIPSE_DE440_TEST_PATH), destination, true));
    addons.RefreshInstalledData();
    ASSERT_TRUE(Wait([&] { return !addons.IsInstallingData(); }));
    EXPECT_TRUE(de->GetLabel().Contains("Installed and verified"));
    EXPECT_TRUE(celestial_navigation::CompactEphemerisEnabled());
    addons.Hide();
    EclipseDialog eclipse(&main, &plugin);
    EXPECT_NE(nullptr, Named<wxButton>(&eclipse, wxString::FromUTF8("Import DE440s…")));
    LunarToolsDialog lunar(&main);
    EXPECT_NE(nullptr, Named<wxButton>(&lunar, "Dut1Download"));
    EXPECT_NE(nullptr, Named<wxCheckBox>(&lunar, "UseCompactEphemeris"));
    dialogs.Unregister(); main.Hide();
  }
  eclipse::SetDut1Update(previousDut1);
  celestial_navigation::SetCompactEphemerisEnabled(previousCompact);
  wxFileName::Rmdir(state, wxPATH_RMDIR_RECURSIVE);
  wxSetAssertHandler(oldAssert); wxTheApp->OnExit(); wxEntryCleanup();
}
