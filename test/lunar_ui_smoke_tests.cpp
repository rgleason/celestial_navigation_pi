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
#include "FindBodyDialog.h"
#include "PlannerDialog.h"
#include "SightDialog.h"
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"
#include "mock_plugin_api.h"
#include "eclipse/dut1.h"
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/dcscreen.h>
#include <wx/dcmemory.h>
#include <wx/image.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {
class InspectFindBody : public FindBodyDialog {
 public:
  using FindBodyDialog::FindBodyDialog;
  using FindBodyDialogBase::m_tAltitude;
  using FindBodyDialogBase::m_tAzimuth;
  using FindBodyDialogBase::m_tLatitude;
  using FindBodyDialogBase::m_tLongitude;
};
class InspectSightDialog : public SightDialog {
 public:
  using SightDialog::SightDialog;
  using SightDialogBase::m_cLimb;
  using SightDialogBase::m_cType;
  using SightDialogBase::m_sCertaintySeconds;
  using SightDialogBase::m_tMeasurement;
};

bool ContainsStaticText(wxWindow* window, const wxString& text) {
  for (auto* child : window->GetChildren()) {
    if (auto* label = dynamic_cast<wxStaticText*>(child))
      if (label->GetLabel().Contains(text)) return true;
    if (ContainsStaticText(child, text)) return true;
  }
  return false;
}

void ExpectUnclippedNonOverlappingChildren(wxWindow* page) {
  auto* scrolled = dynamic_cast<wxScrolledWindow*>(page);
  const wxSize extent = scrolled ? scrolled->GetVirtualSize()
                                  : page->GetClientSize();
  const wxRect client(wxPoint(0, 0), extent);
  std::vector<wxWindow*> visible;
  for (auto* child : page->GetChildren()) {
    if (!child->IsShown() || child->GetSize().x <= 0 ||
        child->GetSize().y <= 0)
      continue;
    // A wxStaticBox is a decorative container whose rectangle deliberately
    // surrounds and intersects the controls managed by its static-box sizer.
    if (dynamic_cast<wxStaticBox*>(child)) continue;
    visible.push_back(child);
    wxRect bounds = child->GetRect();
    if (scrolled) {
      int x = 0, y = 0;
      scrolled->CalcUnscrolledPosition(bounds.x, bounds.y, &x, &y);
      bounds.SetPosition(wxPoint(x, y));
    }
    EXPECT_TRUE(client.Contains(bounds))
        << child->GetClassInfo()->GetClassName() << " at "
        << bounds.GetX() << "," << bounds.GetY()
        << " in " << extent.x << "x" << extent.y;
  }
  for (std::size_t first = 0; first < visible.size(); ++first)
    for (std::size_t second = first + 1; second < visible.size(); ++second)
      EXPECT_FALSE(visible[first]->GetRect().Intersects(
          visible[second]->GetRect()))
          << visible[first]->GetClassInfo()->GetClassName() << " overlaps "
          << visible[second]->GetClassInfo()->GetClassName();
}
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

wxSpinCtrlDouble* FindSpinByTooltip(wxWindow* window,
                                    const wxString& tooltipText) {
  for (auto* child : window->GetChildren()) {
    if (auto* spin = dynamic_cast<wxSpinCtrlDouble*>(child))
      if (spin->GetToolTipText().Contains(tooltipText)) return spin;
    if (auto* result = FindSpinByTooltip(child, tooltipText)) return result;
  }
  return nullptr;
}

wxListCtrl* FindListWithColumn(wxWindow* window, const wxString& heading) {
  for (auto* child : window->GetChildren()) {
    if (auto* list = dynamic_cast<wxListCtrl*>(child)) {
      for (int column = 0; column < list->GetColumnCount(); ++column) {
        wxListItem item;
        item.SetMask(wxLIST_MASK_TEXT);
        if (list->GetColumn(column, item) && item.GetText() == heading)
          return list;
      }
    }
    if (auto* result = FindListWithColumn(child, heading)) return result;
  }
  return nullptr;
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
  const auto previousAssertHandler = wxSetAssertHandler(
      [](const wxString& file, int line, const wxString&,
         const wxString& condition, const wxString& message) {
        ADD_FAILURE() << file.ToStdString() << ":" << line << " "
                      << condition.ToStdString() << " "
                      << message.ToStdString();
      });
  delete wxLog::SetActiveTarget(new wxLogStderr);
  {
    wxFrame frame(nullptr, wxID_ANY, "Lunar UI test");
    {
      wxDateTime recorded;
      ASSERT_TRUE(recorded.ParseISOCombined("2026-08-16T23:59:30"));
      Sight ordinary(Sight::ALTITUDE, "Sun", Sight::LOWER, recorded, 0, 20,
                     0.2);
      ordinary.m_DRBoatPosition = false;
      ordinary.m_DRMagneticAzimuth = false;
      ordinary.m_DRLat = 35;
      ordinary.m_DRLon = -120;
      ordinary.Recompute(120.5);
      const double savedHo = ordinary.m_ObservedAltitude;
      InspectFindBody finder(&frame, ordinary);
      const auto expected = CelestialEphemeris::Evaluate(
          "Sun",
          UtcDateTime::ToInstant(recorded) + wxTimeSpan::Milliseconds(120500),
          35, -120);
      EXPECT_EQ(toSDMM_PlugIn(0, expected.geometricAltitude, true),
                finder.m_tAltitude->GetValue());
      EXPECT_EQ(toSDMM_PlugIn(0, expected.azimuthTrue, true),
                finder.m_tAzimuth->GetValue());
      EXPECT_NE("   N/A", finder.m_tEstimatedHs->GetValue());
      EXPECT_EQ(20, ordinary.m_Measurement);
      EXPECT_EQ(recorded, ordinary.m_DateTime);
      EXPECT_EQ(savedHo, ordinary.m_ObservedAltitude);
      finder.Show();
      finder.Layout();
      for (auto* coordinate : {finder.m_tLatitude, finder.m_tLongitude})
        EXPECT_GE(coordinate->GetClientSize().x,
                  coordinate->GetTextExtent(coordinate->GetValue()).x + 12);
      for (int i = 0; i < 4; ++i) {
        wxTheApp->Yield();
        wxMilliSleep(30);
      }
      for (auto* control :
           {finder.m_tAltitude, finder.m_tAzimuth, finder.m_tEstimatedHs})
        EXPECT_TRUE(wxRect(wxPoint(0, 0), finder.GetClientSize())
                        .Contains(wxRect(
                            finder.ScreenToClient(control->GetScreenPosition()),
                            control->GetSize())));
#ifdef __WXGTK3__
      const auto size = finder.GetSize();
      GtkAllocation requested{0, 0, size.x, size.y};
      gtk_widget_size_allocate(GTK_WIDGET(finder.GetHandle()), &requested);
      auto* surface =
          cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
      auto* cr = cairo_create(surface);
      gtk_widget_draw(GTK_WIDGET(finder.GetHandle()), cr);
      EXPECT_EQ(cairo_surface_write_to_png(surface,
                                           "/tmp/celestial-audit-findbody.png"),
                CAIRO_STATUS_SUCCESS);
      cairo_destroy(cr);
      cairo_surface_destroy(surface);
#endif
      finder.Hide();
    }
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
    sight.m_LunarSeparateTimes = true;
    sight.m_LunarBodyTimeOffsetSeconds = 75;
    sight.m_EyeHeight = 6.1;
    sight.m_Temperature = 23.9;
    sight.m_Pressure = 1019.3;
    sight.m_DRLat = 41 + 22.0 / 60.0;
    sight.m_DRLon = -(71 + 29.0 / 60.0);
    sight.Recompute(0);
    ASSERT_TRUE(sight.m_LunarSolutionValid);
    const auto snapshot = LunarInputSnapshot(sight);
    {
      // Regression for #300: a duplicate lunar converted to Altitude must use
      // the body's recorded Hs/limb and must not reinterpret the lunar UTC
      // search span as plot-time uncertainty.
      Sight converted = sight;
      converted.m_bVisible = false;
      InspectSightDialog properties(&frame, converted, 0);
      properties.m_cType->SetSelection(Sight::ALTITUDE);
      wxCommandEvent typeEvent(wxEVT_CHOICE, properties.m_cType->GetId());
      typeEvent.SetEventObject(properties.m_cType);
      properties.m_cType->ProcessWindowEvent(typeEvent);
      EXPECT_EQ(Sight::ALTITUDE, converted.m_Type);
      EXPECT_EQ(0.0, converted.m_TimeCertainty);
      EXPECT_EQ(sight.m_LunarBodyLimb, converted.m_BodyLimb);
      EXPECT_NEAR(sight.m_LunarBodyAltitude, converted.m_Measurement, 1e-6);
      EXPECT_EQ(UtcDateTime::AddSeconds(sight.m_DateTime, 75),
                converted.m_DateTime);
      EXPECT_EQ(0, properties.m_sCertaintySeconds->GetValue());
      EXPECT_EQ(toSDMM_PlugIn(0, sight.m_LunarBodyAltitude, true),
                properties.m_tMeasurement->GetValue());
      EXPECT_FALSE(converted.IsCalculated());
      converted.RebuildPolygons();
      EXPECT_TRUE(converted.IsCalculated());
      Sight duplicated = converted;
      duplicated.Recompute(0);
      duplicated.RebuildPolygons();
      EXPECT_TRUE(duplicated.IsCalculated());
      EXPECT_EQ(0.0, duplicated.m_TimeCertainty);
    }
    {
      LunarResultsDialog dialog(&frame, sight);
      wxChoice* mode = nullptr;
      wxButton* save = nullptr;
      FindControls(&dialog, &mode, &save);
      ASSERT_NE(nullptr, mode);
      ASSERT_NE(nullptr, save);
      EXPECT_TRUE(save->IsEnabled());
      EXPECT_TRUE(ContainsStaticText(&dialog, "Saved DR used"));
      EXPECT_TRUE(ContainsStaticText(&dialog, "ΔZn"));
      EXPECT_TRUE(ContainsStaticText(&dialog, "effective crossing"));
      dialog.Show();
      dialog.Layout();
      for (int i = 0; i < 4; ++i) {
        wxTheApp->Yield();
        wxMilliSleep(30);
      }
      EXPECT_GE(dialog.GetClientSize().x, 740);
#ifdef __WXGTK3__
      const auto resultSize = dialog.GetSize();
      GtkAllocation resultAllocation{0, 0, resultSize.x, resultSize.y};
      gtk_widget_size_allocate(GTK_WIDGET(dialog.GetHandle()),
                               &resultAllocation);
      auto* resultSurface = cairo_image_surface_create(
          CAIRO_FORMAT_ARGB32, resultSize.x, resultSize.y);
      auto* resultCr = cairo_create(resultSurface);
      gtk_widget_draw(GTK_WIDGET(dialog.GetHandle()), resultCr);
      EXPECT_EQ(cairo_surface_write_to_png(
                    resultSurface, "/tmp/celestial-lunar-results-2.8.5.5.png"),
                CAIRO_STATUS_SUCCESS);
      cairo_destroy(resultCr);
      cairo_surface_destroy(resultSurface);
#endif
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
          EXPECT_TRUE(wxRect(wxPoint(0,0),page->GetVirtualSize()).Contains(child->GetRect()))
              << child->GetClassInfo()->GetClassName() << " "
              << child->GetLabel().ToStdString();
        }
#ifdef __WXGTK3__
        // Wayland intentionally disallows wxScreenDC capture. Render the live
        // GTK widget tree, including native text and controls, to an image.
        GtkAllocation requested{0,0,size.x,size.y};
        gtk_widget_size_allocate(GTK_WIDGET(dialog.GetHandle()),&requested);
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

      // The lunar planner reports measurements and a factual geometric
      // horizon flag, without an undisclosed quality ranking.
      dialog.SelectPageForIntegration(1);
      wxWindow* lunarPlannerPage = notebook->GetPage(1);
      ASSERT_NE(lunarPlannerPage, nullptr);
      auto* lunarPlannerList =
          FindListWithColumn(lunarPlannerPage, "Below horizon");
      ASSERT_NE(lunarPlannerList, nullptr);
      EXPECT_EQ(lunarPlannerList->GetColumnCount(), 13);
      EXPECT_NE(FindListWithColumn(lunarPlannerPage, "Ecliptic lat"), nullptr);
      EXPECT_EQ(FindListWithColumn(lunarPlannerPage, "Quality"), nullptr);
      EXPECT_EQ(FindListWithColumn(lunarPlannerPage, wxString::FromUTF8("0.1′ time")),
                lunarPlannerList);
      ASSERT_GT(lunarPlannerList->GetItemCount(), 0);
      bool sawBelowHorizon = false;
      double previousTime = -1.0;
      for (long index = 0; index < lunarPlannerList->GetItemCount(); ++index) {
        const wxString horizon = lunarPlannerList->GetItemText(index, 1);
        EXPECT_TRUE(horizon.empty() || horizon == "Moon" ||
                    horizon == "Body" || horizon == "Both");
        EXPECT_EQ(lunarPlannerList->GetItemText(index, 5).StartsWith("-"),
                  horizon == "Moon" || horizon == "Both");
        EXPECT_EQ(lunarPlannerList->GetItemText(index, 6).StartsWith("-"),
                  horizon == "Body" || horizon == "Both");
        if (!horizon.empty()) {
          sawBelowHorizon = true;
        } else {
          EXPECT_FALSE(sawBelowHorizon)
              << "above-horizon pair sorted below a flagged pair";
        }
        const wxString time = lunarPlannerList->GetItemText(index, 4);
        if (time == wxString::FromUTF8("—")) continue;
        double seconds = 0.0;
        ASSERT_TRUE(time.BeforeFirst(' ').ToDouble(&seconds));
        if (index > 0 &&
            horizon.empty() == lunarPlannerList->GetItemText(index - 1, 1).empty())
          EXPECT_GE(seconds, previousTime);
        previousTime = seconds;
      }
      EXPECT_TRUE(sawBelowHorizon);
      for (const wxSize size : {wxSize(1120, 780), wxSize(880, 650)}) {
        dialog.SetSize(size);
        dialog.Centre();
        dialog.Layout();
        lunarPlannerPage->Layout();
        for (int i = 0; i < 8; ++i) {
          wxTheApp->Yield();
          wxMilliSleep(30);
        }
        ExpectUnclippedNonOverlappingChildren(lunarPlannerPage);
#ifdef __WXGTK3__
        GtkAllocation requested{0, 0, size.x, size.y};
        gtk_widget_size_allocate(GTK_WIDGET(dialog.GetHandle()), &requested);
        auto* surface =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
        auto* cr = cairo_create(surface);
        gtk_widget_draw(GTK_WIDGET(dialog.GetHandle()), cr);
        const auto path =
            wxString::Format("/tmp/celestial-lunar-planner-%d.png", size.x);
        EXPECT_EQ(cairo_surface_write_to_png(surface, path.utf8_str()),
                  CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
#endif
      }

      // The Sextant Check keeps the prediction engine intact while exposing
      // an independently measured IE.  Verify the complete provenance table
      // and the compact layout, since native Windows controls are wider than
      // their GTK counterparts.
      dialog.SelectPageForIntegration(2);
      wxWindow* sextantPage = notebook->GetPage(2);
      ASSERT_NE(sextantPage, nullptr);
      EXPECT_TRUE(ContainsStaticText(sextantPage, "Measured IE (on arc +)"));
      auto* indexError =
          FindSpinByTooltip(sextantPage, "independently measured index error");
      ASSERT_NE(indexError, nullptr);
      auto* readingList =
          FindListWithColumn(sextantPage, "Predicted apparent");
      ASSERT_NE(readingList, nullptr);
      EXPECT_EQ(readingList->GetColumnCount(), 7);
      for (const wxSize size : {wxSize(1120, 780), wxSize(880, 650)}) {
        dialog.SetSize(size);
        dialog.Centre();
        dialog.Layout();
        sextantPage->Layout();
        for (int i = 0; i < 8; ++i) {
          wxTheApp->Yield();
          wxMilliSleep(30);
        }
        ExpectUnclippedNonOverlappingChildren(sextantPage);
#ifdef __WXGTK3__
        GtkAllocation requested{0, 0, size.x, size.y};
        gtk_widget_size_allocate(GTK_WIDGET(dialog.GetHandle()), &requested);
        auto* surface =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
        auto* cr = cairo_create(surface);
        gtk_widget_draw(GTK_WIDGET(dialog.GetHandle()), cr);
        const auto path =
            wxString::Format("/tmp/celestial-sextant-check-%d.png", size.x);
        EXPECT_EQ(cairo_surface_write_to_png(surface, path.utf8_str()),
                  CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
#endif
      }
      dialog.Hide();

      PlannerDialog planner(&main);
      planner.Show();
      for (int i = 0; i < 4; ++i) {
        wxTheApp->Yield();
        wxMilliSleep(20);
      }
      planner.SelectPageForIntegration(1);
      wxNotebook* plannerNotebook = nullptr;
      for (auto* child : planner.GetChildren())
        if (auto* value = dynamic_cast<wxNotebook*>(child))
          plannerNotebook = value;
      ASSERT_NE(plannerNotebook, nullptr);
      ASSERT_EQ(plannerNotebook->GetSelection(), 1);
      wxWindow* bodiesPage = plannerNotebook->GetPage(1);
      ASSERT_NE(bodiesPage, nullptr);
      for (const wxSize size : {wxSize(1370, 820), wxSize(1024, 700),
                                wxSize(900, 650)}) {
        planner.SetSize(size);
        planner.Centre();
        planner.Layout();
        bodiesPage->Layout();
        for (int i = 0; i < 8; ++i) {
          wxTheApp->Yield();
          wxMilliSleep(30);
        }
        ExpectUnclippedNonOverlappingChildren(bodiesPage);
#ifdef __WXGTK3__
        GtkAllocation requested{0, 0, size.x, size.y};
        gtk_widget_size_allocate(GTK_WIDGET(planner.GetHandle()), &requested);
        auto* surface =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
        auto* cr = cairo_create(surface);
        gtk_widget_draw(GTK_WIDGET(planner.GetHandle()), cr);
        const auto path =
            wxString::Format("/tmp/celestial-planner-bodies-%d.png", size.x);
        EXPECT_EQ(cairo_surface_write_to_png(surface, path.utf8_str()),
                  CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
#endif
      }
      wxChoice* planningMode = nullptr;
      wxCheckBox* showEcliptic = nullptr;
      wxCheckBox* showMoonPath = nullptr;
      for (auto* child : bodiesPage->GetChildren()) {
        if (auto* choice = dynamic_cast<wxChoice*>(child))
          if (choice->GetCount() == 4 &&
              choice->GetString(0) == "Practical Fix") planningMode = choice;
        if (auto* check = dynamic_cast<wxCheckBox*>(child)) {
          if (check->GetLabel() == "Show ecliptic") showEcliptic = check;
          if (check->GetLabel() == "Show Moon path") showMoonPath = check;
        }
      }
      ASSERT_NE(planningMode, nullptr);
      ASSERT_NE(showEcliptic, nullptr);
      ASSERT_NE(showMoonPath, nullptr);
      auto* lunarPairs = FindListWithColumn(bodiesPage, "Moon + body");
      ASSERT_NE(lunarPairs, nullptr);
      for (int mode = 0; mode < 4; ++mode) {
        planningMode->SetSelection(mode);
        wxCommandEvent changed(wxEVT_CHOICE, planningMode->GetId());
        changed.SetEventObject(planningMode);
        planningMode->ProcessWindowEvent(changed);
        for (const wxSize size : {wxSize(1370, 820), wxSize(900, 650)}) {
          planner.SetSize(size);
          planner.Layout();
          bodiesPage->Layout();
          for (int i = 0; i < 5; ++i) { wxTheApp->Yield(); wxMilliSleep(20); }
          ExpectUnclippedNonOverlappingChildren(bodiesPage);
          EXPECT_EQ(mode == 2, lunarPairs->IsShown());
          if (mode == 2) EXPECT_GT(lunarPairs->GetItemCount(), 0);
#ifdef __WXGTK3__
          GtkAllocation allocation{0, 0, size.x, size.y};
          gtk_widget_size_allocate(GTK_WIDGET(planner.GetHandle()),
                                   &allocation);
          auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                      size.x, size.y);
          auto* cr = cairo_create(surface);
          gtk_widget_draw(GTK_WIDGET(planner.GetHandle()), cr);
          const auto path = wxString::Format(
              "/tmp/celestial-planner-mode-%d-%d.png", mode, size.x);
          EXPECT_EQ(cairo_surface_write_to_png(surface, path.utf8_str()),
                    CAIRO_STATUS_SUCCESS);
          cairo_destroy(cr);
          cairo_surface_destroy(surface);
#endif
        }
      }
      planningMode->SetSelection(2);
      wxCommandEvent lunarChanged(wxEVT_CHOICE, planningMode->GetId());
      lunarChanged.SetEventObject(planningMode);
      planningMode->ProcessWindowEvent(lunarChanged);
      planner.SetSize(wxSize(1370, 820));
      planner.Layout();
      bodiesPage->Layout();
      wxNotebook* plotNotebook = nullptr;
      for (auto* child : bodiesPage->GetChildren())
        if (auto* notebook = dynamic_cast<wxNotebook*>(child))
          if (notebook->GetPageCount() == 2 &&
              notebook->GetPageText(0) == "Local sky") plotNotebook = notebook;
      ASSERT_NE(plotNotebook, nullptr);
      plotNotebook->SetSelection(1);
      for (int i = 0; i < 6; ++i) { wxTheApp->Yield(); wxMilliSleep(20); }
      for (int mask = 0; mask < 4; ++mask) {
        showEcliptic->SetValue((mask & 1) != 0);
        showMoonPath->SetValue((mask & 2) != 0);
        for (auto* check : {showEcliptic, showMoonPath}) {
          wxCommandEvent changed(wxEVT_CHECKBOX, check->GetId());
          changed.SetEventObject(check);
          check->ProcessWindowEvent(changed);
        }
        EXPECT_EQ((mask & 1) != 0, showEcliptic->GetValue());
        EXPECT_EQ((mask & 2) != 0, showMoonPath->GetValue());
        plotNotebook->GetCurrentPage()->Refresh();
        plotNotebook->GetCurrentPage()->Update();
#ifdef __WXGTK3__
        for (int i = 0; i < 4; ++i) { wxTheApp->Yield(); wxMilliSleep(20); }
        GtkAllocation allocation{0, 0, 1370, 820};
        gtk_widget_size_allocate(GTK_WIDGET(planner.GetHandle()),
                                 &allocation);
        auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                    1370, 820);
        auto* cr = cairo_create(surface);
        gtk_widget_draw(GTK_WIDGET(planner.GetHandle()), cr);
        const auto path = wxString::Format(
            "/tmp/celestial-planner-sha-overlays-%d.png", mask);
        EXPECT_EQ(cairo_surface_write_to_png(surface, path.utf8_str()),
                  CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
#endif
      }
      auto* scroll = dynamic_cast<wxScrolledWindow*>(bodiesPage);
      ASSERT_NE(scroll, nullptr);
      planner.SetSize(wxSize(900, 650));
      planner.Layout();
      bodiesPage->Layout();
      scroll->Scroll(0, 38);
      for (int i = 0; i < 4; ++i) { wxTheApp->Yield(); wxMilliSleep(20); }
      ExpectUnclippedNonOverlappingChildren(bodiesPage);
#ifdef __WXGTK3__
      GtkAllocation narrow{0, 0, 900, 650};
      gtk_widget_size_allocate(GTK_WIDGET(planner.GetHandle()), &narrow);
      auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                  900, 650);
      auto* cr = cairo_create(surface);
      gtk_widget_draw(GTK_WIDGET(planner.GetHandle()), cr);
      EXPECT_EQ(cairo_surface_write_to_png(surface,
                   "/tmp/celestial-planner-sha-narrow-scrolled.png"),
                CAIRO_STATUS_SUCCESS);
      cairo_destroy(cr);
      cairo_surface_destroy(surface);
      scroll->Scroll(0, 65);
      planner.Refresh();
      planner.Update();
      for (int i = 0; i < 8; ++i) { wxTheApp->Yield(); wxMilliSleep(20); }
      gtk_widget_size_allocate(GTK_WIDGET(planner.GetHandle()), &narrow);
      surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 900, 650);
      cr = cairo_create(surface);
      gtk_widget_draw(GTK_WIDGET(planner.GetHandle()), cr);
      EXPECT_EQ(cairo_surface_write_to_png(surface,
                   "/tmp/celestial-planner-sha-narrow-plot.png"),
                CAIRO_STATUS_SUCCESS);
      cairo_destroy(cr);
      cairo_surface_destroy(surface);
#endif
      planner.Hide();
    }
    SetTestPrivateDataPath(wxEmptyString);
    SetTestPluginDataRoot(wxEmptyString);
    wxFileName::Rmdir(privatePath,wxPATH_RMDIR_RECURSIVE);
  }
  wxTheApp->OnExit();
  wxSetAssertHandler(previousAssertHandler);
  wxEntryCleanup();
}
