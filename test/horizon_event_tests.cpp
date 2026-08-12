#include <gtest/gtest.h>

#include <cmath>

#include "Sight.h"

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

TEST(HorizonEvent, BearingAndEventRecoverAConsistentPosition) {
  Sight sight = MakeHorizonSight();
  double bodyLat, bodyLon;
  sight.BodyLocation(sight.m_CorrectedDateTime, &bodyLat, &bodyLon, 0, 0, 0);
  for (double trace = -165; trace <= 165; trace += 30) {
    const wxRealPoint expected =
        Destination(sight.m_ObservedAltitude, trace, bodyLat, bodyLon);

    double altitude, bearing;
    sight.AltitudeAzimuth(expected.x, expected.y, bodyLat, bodyLon, &altitude,
                          &bearing);
    sight.m_HorizonBearingProvided = true;
    sight.m_HorizonBearing = bearing;

    double actualLat, actualLon;
    ASSERT_TRUE(sight.HorizonEstimatedPosition(&actualLat, &actualLon));
    EXPECT_NEAR(expected.x, actualLat, 0.02) << "trace " << trace;
    EXPECT_NEAR(resolve_heading(expected.y), resolve_heading(actualLon), 0.02)
        << "trace " << trace;
  }
}

TEST(HorizonEvent, ReportsConservativeBearingDominatedUncertainty) {
  Sight sight = MakeHorizonSight();
  sight.m_HorizonBearingProvided = true;
  sight.m_HorizonBearingUncertainty = 1.0;
  sight.m_HorizonAltitudeUncertainty = 10.0;
  sight.m_TimeCertainty = 4.0;
  EXPECT_GT(sight.HorizonEstimateUncertaintyNm(), 60.0);
  EXPECT_LT(sight.HorizonEstimateUncertaintyNm(), 62.0);
}
