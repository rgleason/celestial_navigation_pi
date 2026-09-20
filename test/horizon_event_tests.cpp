#include <gtest/gtest.h>

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/log.h>
#include <wx/scrolwin.h>
#include <wx/stattext.h>

#include "Sight.h"
#include "HorizonEventDialog.h"

#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {

wxRealPoint Destination(double altitude, double trace, double lat, double lon) {
  const double radians = 3.14159265358979323846 / 180.0;
  const double distance = (90.0 - altitude) * radians;
  const double bearing = trace * radians;
  const double lat1 = lat * radians;
  const double lon1 = lon * radians;
  const double lat2 = asin(sin(lat1) * cos(distance) +
                           cos(lat1) * sin(distance) * cos(bearing));
  const double y = sin(bearing) * sin(distance) * cos(lat1);
  const double x = cos(distance) - sin(lat1) * sin(lat2);
  return wxRealPoint(lat2 / radians, (lon1 + atan2(y, x)) / radians);
}

Sight MakeHorizonSight() {
  wxDateTime time;
  EXPECT_TRUE(time.ParseDateTime("2026-06-21 05:00:00"));
  Sight sight(Sight::HORIZON, "Sun", Sight::UPPER, time, 2, 0, 10);
  sight.m_EyeHeight = 0;
  sight.m_Temperature = 10;
  sight.m_Pressure = 1010;
  sight.m_HorizonBearingMagnetic = false;
  sight.m_bMagneticShiftBearing = false;
  sight.Recompute(0);
  return sight;
}

}  // namespace

TEST(HorizonEvent, StandardSeaLevelAltitudeIsAboutMinusFiftyMinutes) {
  Sight sight = MakeHorizonSight();
  EXPECT_GT(sight.m_ObservedAltitude, -0.86);
  EXPECT_LT(sight.m_ObservedAltitude, -0.80);
}

TEST(HorizonEvent, AppliesExplicitMagneticCorrectionsAndWraps) {
  Sight sight = MakeHorizonSight();
  sight.m_HorizonBearingMagnetic = true;
  sight.m_HorizonBearing = 358.0;
  sight.m_HorizonVariation = 3.0;
  sight.m_HorizonDeviation = -1.0;
  EXPECT_NEAR(0.0, sight.HorizonTrueBearing(), 1e-12);
}

TEST(HorizonEvent, BearingAndEventRetainEveryConsistentPosition) {
  Sight sight = MakeHorizonSight();
  double bodyLat, bodyLon;
  sight.BodyLocation(sight.m_CorrectedDateTime, &bodyLat, &bodyLon, 0, 0, 0);
  for (double trace = -179.37; trace < 180; trace += 3.71) {
    const wxRealPoint expected =
        Destination(sight.m_ObservedAltitude, trace, bodyLat, bodyLon);

    double altitude, bearing;
    sight.AltitudeAzimuth(expected.x, expected.y, bodyLat, bodyLon, &altitude,
                          &bearing);
    sight.m_HorizonBearingProvided = true;
    sight.m_HorizonBearing = bearing;
    sight.m_HorizonEvent = bearing < 180 ? Sight::SUNRISE : Sight::SUNSET;
    const auto candidates = sight.HorizonPositionCandidates();
    EXPECT_TRUE(std::any_of(candidates.begin(), candidates.end(),
                           [&](const horizon_position::Position& actual) {
                             return std::abs(actual.latitude - expected.x) <
                                        1e-7 &&
                                    std::abs(resolve_heading(
                                        actual.longitude - expected.y)) < 1e-7;
                           })) << "trace " << trace;
    for (const auto& actual : candidates) {
      double h, az;
      sight.AltitudeAzimuth(actual.latitude, actual.longitude, bodyLat, bodyLon,
                           &h, &az);
      EXPECT_NEAR(sight.m_ObservedAltitude, h, 1e-8);
      EXPECT_NEAR(0.0, resolve_heading(az - bearing), 1e-8);
    }
  }
}

TEST(HorizonEvent, BearingDoesNotAddAFilledUncertaintyDisc) {
  Sight sight = MakeHorizonSight();
  sight.m_HorizonBearingProvided = false;
  sight.RebuildPolygons();
  const auto timeOnlyPoints = sight.GetPoints();
  ASSERT_FALSE(timeOnlyPoints.empty());
  sight.m_HorizonBearingProvided = true;
  sight.m_HorizonBearing = 60.0;
  sight.m_HorizonBearingUncertainty = 2.0;
  sight.RebuildPolygons();
  EXPECT_EQ(timeOnlyPoints, sight.GetPoints());
  ASSERT_EQ(2u, sight.m_HorizonPositions.size());
  EXPECT_NE(wxNOT_FOUND, sight.m_CalcStr.Find("Two nominal positions"));
  EXPECT_NE(wxNOT_FOUND, sight.m_CalcStr.Find("not fixes or confidence areas"));
  EXPECT_EQ(wxNOT_FOUND, sight.m_CalcStr.Find("uncertainty radius"));
}

TEST(HorizonEvent, DoesNotChooseOneOfAmbiguousHawaiiPositions) {
  Sight sight = MakeHorizonSight();
  ASSERT_TRUE(sight.m_DateTime.ParseISOCombined("2025-07-16T16:51:01"));
  sight.m_EyeHeight = 3.0;
  sight.m_Temperature = 11.0;
  sight.m_HorizonBearingProvided = true;
  sight.m_HorizonBearing = 67.0;
  sight.m_HorizonAltitudeUncertainty = 0.1;
  sight.Recompute(0);
  double lat = NAN, lon = NAN;
  EXPECT_FALSE(sight.HorizonEstimatedPosition(&lat, &lon))
      << "A single sunrise bearing cannot choose between hemispheres: "
      << lat << ", " << lon;
  const auto candidates = sight.HorizonPositionCandidates();
  ASSERT_EQ(2u, candidates.size());
  EXPECT_NEAR(19.8762, candidates[0].latitude, 0.001);
  EXPECT_NEAR(-170.3048, candidates[0].longitude, 0.001);
  EXPECT_NEAR(-24.35462, candidates[1].latitude, 0.001);
  for (const auto& point : candidates)
    std::cout << "Bob sunrise candidate: " << point.latitude << ", "
              << point.longitude << '\n';

  // Bob's sunset is earlier on the SAME UTC date: the preceding local evening.
  ASSERT_TRUE(sight.m_DateTime.ParseISOCombined("2025-07-16T05:55:56"));
  sight.m_HorizonEvent = Sight::SUNSET;
  sight.m_HorizonBearing = 293.0;
  sight.Recompute(0);
  const auto sunset = sight.HorizonPositionCandidates();
  ASSERT_EQ(2u, sunset.size());
  for (const auto& point : sunset)
    std::cout << "Bob sunset candidate: " << point.latitude << ", "
              << point.longitude << '\n';
  EXPECT_GT(sunset[0].latitude, 15.0);
  EXPECT_LT(sunset[1].latitude, -15.0);
}

TEST(HorizonEvent, RejectsMismatchedEventAndBearingWithoutLosingTimeLop) {
  Sight sight = MakeHorizonSight();
  sight.m_HorizonBearingProvided = true;
  sight.m_HorizonBearing = 293.0;
  EXPECT_FALSE(sight.HorizonBearingMatchesEvent());
  EXPECT_TRUE(sight.HorizonPositionCandidates().empty());
  EXPECT_NE(wxNOT_FOUND,
            sight.HorizonPositionSummary().Find("does not match"));
  sight.RebuildPolygons();
  EXPECT_FALSE(sight.GetPoints().empty());
  EXPECT_TRUE(sight.m_HorizonPositions.empty());
}

TEST(HorizonEvent, NominalMarkersFollowSavedDrShiftAndClearWhenEdited) {
  Sight sight = MakeHorizonSight();
  sight.m_HorizonBearingProvided = true;
  sight.m_HorizonBearing = 60.0;
  const auto unshifted = sight.HorizonPositionCandidates();
  ASSERT_EQ(2u, unshifted.size());
  sight.m_ShiftNm = 65.5;
  sight.m_ShiftBearing = 255.0;
  sight.m_bMagneticShiftBearing = false;
  sight.RebuildPolygons();
  ASSERT_EQ(2u, sight.m_HorizonPositions.size());
  for (size_t i = 0; i < unshifted.size(); ++i) {
    const auto expected = Destination(90.0 - 65.5 / 60.0, 255.0,
                                      unshifted[i].latitude,
                                      unshifted[i].longitude);
    EXPECT_NEAR(expected.x, sight.m_HorizonPositions[i].latitude, 1e-9);
    EXPECT_NEAR(0.0, resolve_heading(expected.y -
                                     sight.m_HorizonPositions[i].longitude),
                1e-9);
  }
  sight.m_HorizonBearingProvided = false;
  sight.Recompute(0);
  EXPECT_TRUE(sight.m_HorizonPositions.empty());
  sight.RebuildPolygons();
  EXPECT_TRUE(sight.m_HorizonPositions.empty());
}

TEST(HorizonGeometry, HandlesAmbiguousTangentInconsistentAndDegenerateCases) {
  const auto ambiguous = horizon_position::Candidates(20.0, 175.0, 0.0, 60.0);
  ASSERT_EQ(2u, ambiguous.size());
  EXPECT_NEAR(ambiguous[0].latitude, -ambiguous[1].latitude, 1e-9);
  EXPECT_GE(ambiguous[0].longitude, -180.0);
  EXPECT_LE(ambiguous[0].longitude, 180.0);
  const auto tangent = horizon_position::Candidates(30.0, 0.0, 0.0, 60.0);
  ASSERT_EQ(1u, tangent.size());
  EXPECT_NEAR(0.0, tangent[0].latitude, 2e-6);
  EXPECT_TRUE(horizon_position::Candidates(40.0, 0.0, 0.0, 60.0).empty());
  EXPECT_TRUE(horizon_position::Candidates(0.0, 0.0, 0.0, 90.0).empty());
  EXPECT_TRUE(horizon_position::Candidates(NAN, 0.0, 0.0, 60.0).empty());
}

TEST(HorizonGeometry, RecoversSouthernAndEquinoxGeometryAcrossDateLine) {
  Sight sight = MakeHorizonSight();
  for (double declination : {-23.4, -0.01, 0.0, 0.01, 23.4}) {
    for (double trace = -178.73; trace < 180.0; trace += 7.31) {
      const auto expected = Destination(-0.85, trace, declination, 179.8);
      double altitude, azimuth;
      sight.AltitudeAzimuth(expected.x, expected.y, declination, 179.8,
                           &altitude, &azimuth);
      const auto positions = horizon_position::Candidates(
          declination, 179.8, altitude, azimuth);
      EXPECT_TRUE(std::any_of(positions.begin(), positions.end(),
                             [&](const horizon_position::Position& actual) {
                               return std::abs(actual.latitude - expected.x) <
                                          1e-7 &&
                                      std::abs(resolve_heading(
                                          actual.longitude - expected.y)) <
                                          1e-7;
                             })) << declination << ", trace " << trace;
    }
  }
}

TEST(HorizonEventUi, ExplainsAmbiguousHawaiiPositionsInCompactDialog) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Run alone with CELESTIAL_RUN_UI_TESTS=1 and a display";
  int argc = 1;
  char name[] = "celestial-horizon-ui-test";
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
  {
    wxFrame frame(nullptr, wxID_ANY, "Horizon test host");
    Sight sight = MakeHorizonSight();
    ASSERT_TRUE(sight.m_DateTime.ParseISOCombined("2025-07-16T16:51:01"));
    sight.m_EyeHeight = 3.0;
    sight.m_Temperature = 11.0;
    sight.m_HorizonBearingProvided = true;
    sight.m_HorizonBearing = 67.0;
    sight.m_HorizonAltitudeUncertainty = 0.1;
    HorizonEventDialog dialog(&frame, sight, 0, "Test UTC source");
    dialog.SetSize(700, 760);
    dialog.Show();
    dialog.Layout();
    wxTheApp->Yield(true);
    wxScrolledWindow* scroller = nullptr;
    for (auto* child : dialog.GetChildren())
      if (auto* scroll = dynamic_cast<wxScrolledWindow*>(child))
        scroller = scroll;
    ASSERT_NE(nullptr, scroller);
    wxStaticText* preview = nullptr;
    for (auto* child : scroller->GetChildren())
      if (auto* label = dynamic_cast<wxStaticText*>(child))
        if (label->GetLabel().Find("Two nominal positions") != wxNOT_FOUND)
          preview = label;
    ASSERT_NE(nullptr, preview);
    wxString previewText = preview->GetLabel();
    previewText.Replace("\n", " ");
    EXPECT_NE(wxNOT_FOUND,
              previewText.Find("not fixes or confidence areas"));
    EXPECT_NE(wxNOT_FOUND, previewText.Find("very optimistic"));
    EXPECT_LE(preview->GetSize().x, scroller->GetClientSize().x);
    scroller->Scroll(0, scroller->GetVirtualSize().y);
    wxTheApp->Yield(true);
#ifdef __WXGTK3__
    const auto size = dialog.GetSize();
    GtkAllocation allocation{0, 0, size.x, size.y};
    gtk_widget_size_allocate(GTK_WIDGET(dialog.GetHandle()), &allocation);
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                size.x, size.y);
    auto* cr = cairo_create(surface);
    gtk_widget_draw(GTK_WIDGET(dialog.GetHandle()), cr);
    EXPECT_EQ(cairo_surface_write_to_png(surface,
                                         "/tmp/celestial-horizon-289.png"),
              CAIRO_STATUS_SUCCESS);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
#endif
    dialog.Hide();
  }
  wxSetAssertHandler(oldAssert);
  wxTheApp->OnExit();
  wxEntryCleanup();
}
