#include <gtest/gtest.h>
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <cstdlib>
#include "NauticalTimeCtrl.h"
#include "LunarResultsDialog.h"
#include "LunarSolutionRecord.h"

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
  }
  wxTheApp->OnExit();
  wxEntryCleanup();
}
