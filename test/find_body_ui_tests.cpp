#include <gtest/gtest.h>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filename.h>
#include <wx/ffile.h>
#include <wx/frame.h>
#include <wx/listctrl.h>
#include <wx/log.h>
#include <wx/modalhook.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>
#include <cstdlib>
#include <functional>
#include "CelestialNavigationDialog.h"
#include "FindBodyDialog.h"
#include "SightDialog.h"
#include "UtcDateTime.h"
#include "WaypointPickerDialog.h"
#include "mock_plugin_api.h"
#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {
template <class T>
T* Find(wxWindow* root, const wxString& label) {
  for (auto* child : root->GetChildren()) {
    if (auto* value = dynamic_cast<T*>(child))
      if (value->GetLabel() == label) return value;
    if (auto* value = Find<T>(child, label)) return value;
  }
  return nullptr;
}
wxTextCtrl* Coordinate(wxWindow* root, const wxString& label) {
  for (auto* child : root->GetChildren()) {
    if (auto* text = dynamic_cast<wxTextCtrl*>(child))
      if (text->GetParent()->GetLabel() == label) return text;
    if (auto* value = Coordinate(child, label)) return value;
  }
  return nullptr;
}
wxChoice* PositionSource(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (auto* choice = dynamic_cast<wxChoice*>(child))
      if (choice->GetName() == "Find position source") return choice;
    if (auto* value = PositionSource(child)) return value;
  }
  return nullptr;
}
void SelectPositionSource(wxWindow* root, int source) {
  auto* choice = PositionSource(root);
  ASSERT_NE(nullptr, choice);
  choice->SetSelection(source);
  wxCommandEvent change(wxEVT_CHOICE, choice->GetId());
  change.SetEventObject(choice);
  choice->ProcessWindowEvent(change);
}
wxString SavedXml(const wxString& path) {
  if (!wxFileExists(path)) return wxString();
  wxFFile file(path, "r");
  wxString contents;
  EXPECT_TRUE(file.ReadAll(&contents));
  return contents;
}
void Click(wxWindow* root, const wxString& label) {
  auto* button = Find<wxButton>(root, label);
  ASSERT_NE(nullptr, button) << label.ToStdString();
  ASSERT_TRUE(button->IsEnabled());
  wxCommandEvent event(wxEVT_BUTTON, button->GetId());
  event.SetEventObject(button);
  button->ProcessWindowEvent(event);
}
class Main : public CelestialNavigationDialog {
public:
  using CelestialNavigationDialog::CelestialNavigationDialog;
  using CelestialNavigationDialogBase::m_lSights;
};
class Hook : public wxModalDialogHook {
public:
  std::function<void(SightDialog*)> properties;
  std::function<void(FindBodyDialog*)> finder;
  std::function<void(WaypointPickerDialog*)> waypoint;
  int Enter(wxDialog* dialog) override {
    if (auto* value = dynamic_cast<WaypointPickerDialog*>(dialog))
      wxTheApp->CallAfter([=] {
        waypoint(value);
        if (value->IsModal()) value->EndModal(wxID_CANCEL);
      });
    else if (auto* value = dynamic_cast<FindBodyDialog*>(dialog))
      wxTheApp->CallAfter([=] {
        finder(value);
        if (value->IsModal()) {
          ADD_FAILURE() << "Find action failed to dismiss the popup";
          value->EndModal(wxID_CANCEL);
        }
      });
    else if (auto* value = dynamic_cast<SightDialog*>(dialog))
      wxTheApp->CallAfter([=] {
        properties(value);
        if (value->IsModal()) {
          ADD_FAILURE() << "Sight action failed to dismiss properties";
          value->EndModal(wxID_CANCEL);
        }
      });
    else {
      ADD_FAILURE() << "Unexpected modal dialog: " << dialog->GetTitle();
      return wxID_CANCEL;
    }
    return wxID_NONE;
  }
};
void OpenFind(SightDialog* properties, int route) {
  wxCommandEvent event;
  if (route == 0 || route == 3)
    properties->OnFindBody(event);
  else if (route == 1)
    properties->OnFindLunarMoon(event);
  else
    properties->OnFindLunarBody(event);
}
}  // namespace

TEST(FindBodyUi, IndependentActionsThroughAllThreeSightRoutes) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Run alone with CELESTIAL_RUN_UI_TESTS=1 and a display";
  int argc = 1;
  char name[] = "celestial-find-ui-test";
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
  const wxString state = wxFileName::CreateTempFileName("celestial-find-test-");
  ASSERT_TRUE(wxRemoveFile(state));
  ASSERT_TRUE(wxFileName::Mkdir(state + "/plugins/celestial_navigation", 0777,
                                wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(state);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  wxInitAllImageHandlers();
  {
    wxFrame frame(nullptr, wxID_ANY, "Find workflow test");
    celestial_navigation_pi plugin(nullptr);
    PlugIn_Position_Fix_Ex boat{};
    boat.Lat = 53.0;
    boat.Lon = -4.0;
    plugin.SetPositionFixEx(boat);
    plugin.SetCursorLatLon(49.0, -4.0);
    Main main(&frame, &plugin);
    main.SetSize(wxSize(1200, 800));
    main.Show();
    main.Layout();
    const wxRect sightWindow(wxPoint(0, 0), main.GetClientSize());
    for (const auto& label : {
             "New", "Duplicate", "Edit", "Delete", "Delete All",
             "Horizon Event...", "Coastal Sextant...", "Sun && Moon...",
             "Lunar Tools...", "Analyze Sights...", "Fix...", "Clock Offset",
             "Eclipses...", "Generate Almanac...", "Documentation",
             "PDF Documentation"}) {
      auto* button = Find<wxButton>(&main, label);
      ASSERT_NE(nullptr, button) << label;
      EXPECT_TRUE(button->IsShown()) << label;
      EXPECT_TRUE(sightWindow.Contains(button->GetRect())) << label;
    }
    auto* hideSights = Find<wxToggleButton>(&main, "Hide Sights");
    ASSERT_NE(nullptr, hideSights);
    EXPECT_TRUE(sightWindow.Contains(hideSights->GetRect()));
    EXPECT_NE(nullptr, Find<wxToggleButton>(&main, "Hide Time"));
    EXPECT_LT(Find<wxButton>(&main, "New")->GetPosition().x,
              Find<wxButton>(&main, "Fix...")->GetPosition().x);
    EXPECT_LT(Find<wxButton>(&main, "New")->GetPosition().y,
              Find<wxButton>(&main, "Edit")->GetPosition().y);
    EXPECT_LT(Find<wxButton>(&main, "Edit")->GetPosition().y,
              Find<wxButton>(&main, "Analyze Sights...")->GetPosition().y);
#ifdef __WXGTK3__
    const auto sightsSize = main.GetSize();
    GtkAllocation sightsAllocation{0, 0, sightsSize.x, sightsSize.y};
    gtk_widget_size_allocate(GTK_WIDGET(main.GetHandle()), &sightsAllocation);
    auto* sightsSurface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, sightsSize.x, sightsSize.y);
    auto* sightsCr = cairo_create(sightsSurface);
    gtk_widget_draw(GTK_WIDGET(main.GetHandle()), sightsCr);
    EXPECT_EQ(cairo_surface_write_to_png(
                  sightsSurface, "/tmp/celestial-sights-2859.png"),
              CAIRO_STATUS_SUCCESS);
    cairo_destroy(sightsCr);
    cairo_surface_destroy(sightsSurface);
#endif
    main.Hide();
    wxDateTime fixEpoch;
    ASSERT_TRUE(fixEpoch.ParseISOCombined("2024-06-13T18:00:00"));
    main.SetLastFix(48.2, -5.3, UtcDateTime::ToInstant(fixEpoch));
    SetTestWaypoints({{"chester", "Chester", 53.19, -2.89}});
    auto* timeToggle = Find<wxToggleButton>(&main, "Hide Time");
    ASSERT_NE(nullptr, timeToggle);
    timeToggle->SetValue(false);
    wxCommandEvent toggleTime(wxEVT_TOGGLEBUTTON, timeToggle->GetId());
    toggleTime.SetEventObject(timeToggle);
    timeToggle->ProcessWindowEvent(toggleTime);
    EXPECT_EQ("Show Time", timeToggle->GetLabel());
    timeToggle->SetValue(true);
    timeToggle->ProcessWindowEvent(toggleTime);
    EXPECT_EQ("Hide Time", timeToggle->GetLabel());
    Hook hook;
    hook.Register();
    // 0 close only, 1 window X, 2 Escape, 3 reset, 4 copy twice,
    // 5 no estimate, 6 copy then cancel the outer Sight Properties,
    // 7 Cancel after a typo/live toggle, 8/9/10 copy then Cancel/X/Escape.
    for (int route = 0; route < 4; ++route)
      for (int action = 0; action < 11; ++action) {
        if (route == 3 && (action == 4 || action == 6 || action >= 8))
          continue;
        SCOPED_TRACE(::testing::Message()
                     << "route=" << route << " action=" << action);
        wxDateTime time;
        ASSERT_TRUE(time.ParseISOCombined("2024-06-13T19:26:00"));
        Sight initial(route == 0   ? Sight::ALTITUDE
                      : route == 3 ? Sight::AZIMUTH
                                   : Sight::LUNAR,
                      "Sun",
                      route == 0   ? Sight::LOWER
                      : route == 3 ? Sight::CENTER
                                   : Sight::LUNAR_NEAR,
                      time, 7200, route == 0 ? 51.5 : 85 + 40.3 / 60, 0.2);
        initial.m_DRLat = 41 + 22.0 / 60;
        initial.m_DRLon = -(71 + 29.0 / 60);
        initial.m_DRBoatPosition = false;
        initial.m_DRMagneticAzimuth = false;
        initial.m_LunarMoonAltitude = 34 + 34.0 / 60;
        initial.m_LunarBodyAltitude = 51 + 58.0 / 60;
        initial.m_LunarMoonLimb = Sight::UPPER;
        initial.m_LunarBodyLimb = Sight::LOWER;
        initial.m_EyeHeight = 6.1;
        initial.m_bVisible = false;
        initial.Recompute(37);
        main.m_Sights.clear();
        main.m_Sights.push_back(initial);
        main.m_lSights->DeleteAllItems();
        main.m_lSights->InsertItem(0, "Test sight");
        main.m_lSights->SetItemState(0, wxLIST_STATE_SELECTED,
                                     wxLIST_STATE_SELECTED);
        double expectedLat = 42, expectedLon = -70, copied = NAN;
        const wxString xmlPath = state + "/plugins/celestial_navigation/Sights.xml";
        const wxString xmlBefore = SavedXml(xmlPath);
        int visits = 0;
        hook.waypoint = [&](WaypointPickerDialog* picker) {
          wxListCtrl* list = nullptr;
          for (auto* child : picker->GetChildren())
            if ((list = dynamic_cast<wxListCtrl*>(child))) break;
          ASSERT_NE(nullptr, list);
          ASSERT_EQ(1, list->GetItemCount());
          wxListEvent selection(wxEVT_LIST_ITEM_SELECTED, list->GetId());
          wxListItem item;
          item.SetData(static_cast<long>(list->GetItemData(0)));
          selection.SetItem(item);
          list->ProcessWindowEvent(selection);
          picker->EndModal(wxID_OK);
        };
        hook.finder = [&](FindBodyDialog* dialog) {
          ++visits;
          auto* lat = Coordinate(dialog, "Latitude");
          auto* lon = Coordinate(dialog, "Longitude");
          ASSERT_NE(nullptr, lat);
          ASSERT_NE(nullptr, lon);
          ASSERT_EQ(wxID_CLOSE, dialog->GetAffirmativeId());
          auto* cancel = Find<wxButton>(dialog, "Cancel");
          ASSERT_NE(nullptr, cancel);
          ASSERT_EQ(cancel->GetId(), dialog->GetEscapeId());
          auto* ho = Coordinate(dialog, "Altitude (Ho)");
          ASSERT_NE(nullptr, ho);
          EXPECT_FALSE(ho->IsEditable());
          EXPECT_EQ(route == 3 ? wxString("N/A")
                               : toSDMM_PlugIn(
                                     0, dialog->m_Sight.m_ObservedAltitude, true),
                    ho->GetValue());
          if (visits == 2) {
            EXPECT_NEAR(expectedLat, dialog->m_Sight.m_DRLat, 1e-6);
            EXPECT_NEAR(expectedLon, dialog->m_Sight.m_DRLon, 1e-6);
            Click(dialog, "Close");
            return;
          }
          if (route == 0 && action == 0) {
            auto* source = PositionSource(dialog);
            ASSERT_NE(nullptr, source);
            EXPECT_EQ(5u, source->GetCount());
            EXPECT_EQ(0, source->GetSelection());
            SelectPositionSource(dialog, 2);
            EXPECT_NEAR(49.0, dialog->m_Sight.m_DRLat, 1e-6);
            EXPECT_FALSE(lat->IsEnabled());
            SelectPositionSource(dialog, 3);
            EXPECT_NEAR(48.2, dialog->m_Sight.m_DRLat, 1e-6);
            EXPECT_NE(nullptr, Find<wxStaticText>(
                                   dialog, "Fix epoch: 2024-06-13 18:00:00 UTC"));
#ifdef __WXGTK3__
            dialog->Layout();
            const auto fixSize = dialog->GetSize();
            GtkAllocation fixAllocation{0, 0, fixSize.x, fixSize.y};
            gtk_widget_size_allocate(GTK_WIDGET(dialog->GetHandle()),
                                     &fixAllocation);
            auto* fixSurface = cairo_image_surface_create(
                CAIRO_FORMAT_ARGB32, fixSize.x, fixSize.y);
            auto* fixCr = cairo_create(fixSurface);
            gtk_widget_draw(GTK_WIDGET(dialog->GetHandle()), fixCr);
            EXPECT_EQ(cairo_surface_write_to_png(
                          fixSurface, "/tmp/find-body-2859-fix.png"),
                      CAIRO_STATUS_SUCCESS);
            cairo_destroy(fixCr);
            cairo_surface_destroy(fixSurface);
#endif
            SelectPositionSource(dialog, 4);
            EXPECT_NEAR(53.19, dialog->m_Sight.m_DRLat, 1e-6);
            EXPECT_NEAR(-2.89, dialog->m_Sight.m_DRLon, 1e-6);
            SelectPositionSource(dialog, 0);
            EXPECT_TRUE(lat->IsEnabled());
          }
          lat->SetValue("42 N");
          lon->SetValue("70 W");
          wxCommandEvent enter(wxEVT_TEXT_ENTER, lat->GetId());
          enter.SetEventObject(lat);
          lat->ProcessWindowEvent(enter);
          EXPECT_TRUE(dialog->IsShown());
          EXPECT_NEAR(initial.m_Measurement, main.m_Sights[0].m_Measurement,
                      1e-6);
          if (route == 3) {
            EXPECT_FALSE(
                Find<wxButton>(dialog, "Copy estimated Hs")->IsEnabled());
          }
          if (action == 3) {
            SelectPositionSource(dialog, 1);
            Click(dialog, "Reset position");
            EXPECT_EQ(0, PositionSource(dialog)->GetSelection());
            EXPECT_TRUE(lat->IsEnabled());
            EXPECT_TRUE(dialog->IsShown());
            expectedLat = initial.m_DRLat;
            expectedLon = initial.m_DRLon;
          }
          if (action == 4 || action == 6 || action >= 8) {
            copied = fromDMM_Plugin(dialog->m_tEstimatedHs->GetValue());
            Click(dialog, "Copy estimated Hs");
            EXPECT_TRUE(dialog->IsShown());
            EXPECT_EQ(toSDMM_PlugIn(
                          0, dialog->m_Sight.m_ObservedAltitude, true),
                      ho->GetValue());
            const auto& editing = main.m_Sights[0];
            EXPECT_NEAR(copied,
                        route == 0   ? editing.m_Measurement
                        : route == 1 ? editing.m_LunarMoonAltitude
                                     : editing.m_LunarBodyAltitude,
                        1e-6);
            lat->SetValue("43 N");
            copied = fromDMM_Plugin(dialog->m_tEstimatedHs->GetValue());
            Click(dialog, "Copy estimated Hs");
            EXPECT_TRUE(dialog->IsShown());
            if (action == 4 || action == 6) {
              // Reset affects position only, never undoes an explicit copy.
              Click(dialog, "Reset position");
              EXPECT_TRUE(dialog->IsShown());
              expectedLat = initial.m_DRLat;
              expectedLon = initial.m_DRLon;
            }
          }
          if (action == 5) {
            double gpLat, gpLon;
            dialog->m_Sight.BodyLocation(dialog->m_Sight.m_CorrectedDateTime,
                                         &gpLat, &gpLon, nullptr, nullptr,
                                         nullptr);
            expectedLat = -gpLat;
            expectedLon = std::remainder(gpLon + 180, 360);
            lat->SetValue(toSDMM_PlugIn(1, expectedLat, true));
            lon->SetValue(toSDMM_PlugIn(2, expectedLon, true));
            EXPECT_FALSE(
                Find<wxButton>(dialog, "Copy estimated Hs")->IsEnabled());
            EXPECT_TRUE(Find<wxButton>(dialog, "Close")->IsEnabled());
          }
#ifdef __WXGTK3__
          if (route == 0 && action == 0) {
            dialog->Layout();
            const auto size = dialog->GetSize();
            GtkAllocation allocation{0, 0, size.x, size.y};
            gtk_widget_size_allocate(GTK_WIDGET(dialog->GetHandle()),
                                     &allocation);
            auto* surface =
                cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
            auto* cr = cairo_create(surface);
            gtk_widget_draw(GTK_WIDGET(dialog->GetHandle()), cr);
            EXPECT_EQ(
                cairo_surface_write_to_png(surface, "/tmp/find-body-2859.png"),
                CAIRO_STATUS_SUCCESS);
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            for (const auto& label :
                 {"Reset position", "Copy estimated Hs", "Cancel", "Close"}) {
              auto* button = Find<wxButton>(dialog, label);
              EXPECT_TRUE(wxRect(wxPoint(0, 0), dialog->GetClientSize())
                              .Contains(button->GetRect()));
            }
            EXPECT_LT(ho->GetScreenPosition().y,
                      Find<wxButton>(dialog, "Copy estimated Hs")
                          ->GetScreenPosition().y);
            EXPECT_LT(dialog->m_tEstimatedHs->GetScreenPosition().x,
                      Find<wxButton>(dialog, "Copy estimated Hs")
                          ->GetScreenPosition().x);
          }
#endif
          if (action == 1 || action == 2 || action >= 7) {
            expectedLat = initial.m_DRLat;
            expectedLon = initial.m_DRLon;
            if (action == 7) {
              SelectPositionSource(dialog, 1);
            }
            lat->SetValue("");  // An accidental blank must not be committed.
          }
          EXPECT_EQ(xmlBefore, SavedXml(xmlPath));
          if (action == 1 || action == 9)
            dialog->Close();
          else if (action == 2 || action == 10) {
            wxKeyEvent escape(wxEVT_CHAR_HOOK);
            escape.m_keyCode = WXK_ESCAPE;
            dialog->ProcessWindowEvent(escape);
          } else if (action == 7 || action == 8)
            Click(dialog, "Cancel");
          else
            Click(dialog, "Close");
        };
        hook.properties = [&](SightDialog* properties) {
          OpenFind(properties, route);
          EXPECT_EQ(xmlBefore, SavedXml(xmlPath));
          OpenFind(properties,
                   route);  // Position survives closing/reopening Find.
          EXPECT_EQ(xmlBefore, SavedXml(xmlPath));
          wxCommandEvent done(wxEVT_BUTTON,
                              action == 6 ? wxID_CANCEL : wxID_OK);
          properties->ProcessWindowEvent(done);
        };
        wxListEvent edit(wxEVT_LIST_ITEM_ACTIVATED, main.m_lSights->GetId());
        edit.SetEventObject(main.m_lSights);
        main.m_lSights->ProcessWindowEvent(edit);
        EXPECT_EQ(2, visits);
        const Sight& saved = main.m_Sights[0];
        if (action == 6) {
          expectedLat = initial.m_DRLat;
          expectedLon = initial.m_DRLon;
        }
        EXPECT_NEAR(expectedLat, saved.m_DRLat, 1e-6);
        EXPECT_NEAR(expectedLon, saved.m_DRLon, 1e-6);
        EXPECT_EQ(initial.m_DRBoatPosition, saved.m_DRBoatPosition);
        EXPECT_EQ(initial.m_DateTime, saved.m_DateTime);
        const bool keptCopy = action == 4 || action >= 8;
        EXPECT_NEAR(keptCopy && route == 0 ? copied : initial.m_Measurement,
                    saved.m_Measurement, 1e-6);
        EXPECT_NEAR(
            keptCopy && route == 1 ? copied : initial.m_LunarMoonAltitude,
            saved.m_LunarMoonAltitude, 1e-6);
        EXPECT_NEAR(
            keptCopy && route == 2 ? copied : initial.m_LunarBodyAltitude,
            saved.m_LunarBodyAltitude, 1e-6);
      }
  }
  SetTestPrivateDataPath(wxString());
  SetTestWaypoints({});
  SetTestPluginDataRoot(wxString());
  wxSetAssertHandler(oldAssert);
  wxTheApp->OnExit();
  wxEntryCleanup();
}
