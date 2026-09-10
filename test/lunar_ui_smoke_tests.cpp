#include <gtest/gtest.h>
#include <wx/app.h>
#include <wx/log.h>
#include <wx/frame.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <cstdlib>
#include "NauticalTimeCtrl.h"
#include "LunarResultsDialog.h"
#include "LunarSolutionRecord.h"
#include "CelestialNavigationDialog.h"
#include "LunarToolsDialog.h"
#include "mock_plugin_api.h"
#include "eclipse/dut1.h"
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/stattext.h>
#include <wx/dcscreen.h>
#include <wx/dcmemory.h>
#include <wx/image.h>
#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {
void FindControls(wxWindow* window, wxChoice** mode, wxButton** save) {
  for (auto* child : window->GetChildren()) {
    if (auto* choice = dynamic_cast<wxChoice*>(child))
      if (choice->GetCount() == 2 &&
          choice->GetString(1).Contains("Check at entered"))
        *mode = choice;
    if (auto* button = dynamic_cast<wxButton*>(child))
      if (button->GetLabel() == "Save lunar solution") *save = button;
    FindControls(child, mode, save);
  }
}
}  // namespace

// Explicit opt-in: run alone with a display, separate from non-GUI tests.
TEST(LunarUiSmoke, TimeEntryAndResultModesPreserveRecordedInputs) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Set CELESTIAL_RUN_UI_TESTS=1 and run this test alone with "
                    "a display";
  int argc = 1;
  char name[] = "celestial-lunar-ui-test";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  delete wxLog::SetActiveTarget(new wxLogStderr);
  {
    wxFrame frame(nullptr, wxID_ANY, "Lunar UI test");
    wxDateTime time;
    ASSERT_TRUE(time.ParseISOCombined("2024-06-13T19:26:00"));
    NauticalTimeCtrl time_control(&frame, wxID_ANY, time);
    EXPECT_EQ(19, time_control.GetValue().GetHour());
    EXPECT_EQ(26, time_control.GetValue().GetMinute());
    Sight sight(Sight::LUNAR, "Sun", Sight::LUNAR_NEAR, time, 7200,
                85 + 40.3 / 60.0, 0.2);
    sight.m_LunarMoonAltitude = 34 + 34.0 / 60.0;
    sight.m_LunarBodyAltitude = 51 + 58.0 / 60.0;
    sight.m_LunarMoonLimb = Sight::UPPER;
    sight.m_LunarBodyLimb = Sight::LOWER;
    sight.m_LunarBodyDistanceLimb = Sight::LUNAR_NEAR;
    sight.m_EyeHeight = 6.1;
    sight.m_Temperature = 23.9;
    sight.m_Pressure = 1019.3;
    sight.m_DRLat = 41 + 22.0 / 60.0;
    sight.m_DRLon = -(71 + 29.0 / 60.0);
    sight.Recompute(0);
    ASSERT_TRUE(sight.m_LunarSolutionValid);
    const auto snapshot = LunarInputSnapshot(sight);
    {
      LunarResultsDialog dialog(&frame, sight);
      wxChoice* mode = nullptr;
      wxButton* save = nullptr;
      FindControls(&dialog, &mode, &save);
      ASSERT_NE(nullptr, mode);
      ASSERT_NE(nullptr, save);
      EXPECT_TRUE(save->IsEnabled());
      for (int selection : {1, 0, 1}) {
        mode->SetSelection(selection);
        wxCommandEvent event(wxEVT_CHOICE, mode->GetId());
        event.SetEventObject(mode);
        mode->ProcessWindowEvent(event);
        EXPECT_EQ(selection == 0, save->IsEnabled());
        EXPECT_EQ(snapshot, LunarInputSnapshot(sight));
      }
    }
    // Exercise the actual main and Lunar Tools dialogs, with isolated host
    // state. No installed sights, configuration or network are touched.
    const wxString privatePath=wxFileName::CreateTempFileName("celestial-ui-state-");
    ASSERT_TRUE(wxRemoveFile(privatePath));
    ASSERT_TRUE(wxFileName::Mkdir(privatePath+"/plugins/celestial_navigation",0777,wxPATH_MKDIR_FULL));
    SetTestPrivateDataPath(privatePath);
    SetTestPluginDataRoot(wxFileName(__FILE__).GetPath()+"/..");
    GetOCPNConfigObject()->Write("/PlugIns/CelestialNavigation/ShowTimeIntegrity",false);
    wxInitAllImageHandlers();
    {
      celestial_navigation_pi plugin(nullptr);
      CelestialNavigationDialog main(&frame,&plugin);
      LunarToolsDialog dialog(&main);
      wxNotebook* notebook=nullptr;
      for (auto* child:dialog.GetChildren())
        if (auto* value=dynamic_cast<wxNotebook*>(child)) notebook=value;
      ASSERT_NE(notebook,nullptr);
      ASSERT_EQ(notebook->GetPageCount(),4u);
      EXPECT_EQ(notebook->GetPageText(3),"Advanced");
      notebook->SetSelection(3);
      auto* page=dynamic_cast<wxScrolledWindow*>(notebook->GetPage(3));
      ASSERT_NE(page,nullptr);
      const int calls=TestDownloadCalls();
      dialog.Show();
      for (const wxSize size : {wxSize(1120,780),wxSize(880,650)}) {
        dialog.SetSize(size); dialog.Centre(); dialog.Layout();
        for (int i=0;i<8;++i) { wxTheApp->Yield(); wxMilliSleep(30); }
        EXPECT_EQ(TestDownloadCalls(),calls); // Opening/resizing never downloads.
        EXPECT_LE(page->GetVirtualSize().x,page->GetClientSize().x);
        for (auto* child:page->GetChildren()) {
          if (!child->IsShown() || child->GetSize().y==0) continue;
          EXPECT_TRUE(wxRect(wxPoint(0,0),page->GetClientSize()).Contains(child->GetRect()))
              << child->GetLabel().ToStdString();
        }
#ifdef __WXGTK3__
        // Wayland intentionally disallows wxScreenDC capture. Render the live
        // GTK widget tree, including native text and controls, to an image.
        auto* surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,size.x,size.y);
        auto* cr=cairo_create(surface);
        gtk_widget_draw(GTK_WIDGET(dialog.GetHandle()),cr);
        const auto path=wxString::Format("/tmp/celestial-advanced-%d.png",size.x);
        EXPECT_EQ(cairo_surface_write_to_png(surface,path.utf8_str()),CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr); cairo_surface_destroy(surface);
#else
        wxScreenDC screen;
        wxBitmap bitmap(dialog.GetSize());
        wxMemoryDC memory(bitmap);
        const auto origin=dialog.GetScreenPosition();
        ASSERT_TRUE(memory.Blit(0,0,bitmap.GetWidth(),bitmap.GetHeight(),&screen,origin.x,origin.y));
        memory.SelectObject(wxNullBitmap);
        ASSERT_TRUE(bitmap.SaveFile(wxString::Format("/tmp/celestial-advanced-%d.png",size.x),wxBITMAP_TYPE_PNG));
#endif
      }
      wxButton* download=nullptr;
      for (auto* child:page->GetChildren())
        if (auto* button=dynamic_cast<wxButton*>(child))
          if (button->GetLabel().StartsWith("Check / download")) download=button;
      ASSERT_NE(download,nullptr);
      const auto previous=eclipse::GetDut1Update();
      wxCommandEvent event(wxEVT_BUTTON,download->GetId());
      event.SetEventObject(download); download->ProcessWindowEvent(event);
      EXPECT_EQ(TestDownloadCalls(),calls+1);
      EXPECT_TRUE(download->IsEnabled());
      EXPECT_EQ(eclipse::GetDut1Update(),previous);
      bool failureShown=false;
      for (auto* child:page->GetChildren())
        if (child->GetLabel().Contains("failed or was cancelled")) failureShown=true;
      EXPECT_TRUE(failureShown);
      dialog.Hide();
    }
    SetTestPrivateDataPath(wxEmptyString);
    SetTestPluginDataRoot(wxEmptyString);
    wxFileName::Rmdir(privatePath,wxPATH_RMDIR_RECURSIVE);
  }
  wxTheApp->OnExit();
  wxEntryCleanup();
}
