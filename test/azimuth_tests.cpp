#include <gtest/gtest.h>

#include "Sight.h"

TEST(AzimuthSight, IsImplementedAndIdentifiesItsNavigationMeaning) {
  wxDateTime utc;
  ASSERT_TRUE(utc.ParseDateTime("2026-08-14 12:00:00"));
  Sight sight(Sight::AZIMUTH, "Sun", Sight::CENTER, utc, 1.0, 725.0, 2.0);
  sight.m_bMagneticNorth = false;
  sight.Recompute(0);
  EXPECT_TRUE(sight.IsCalculated());
  EXPECT_NEAR(sight.m_Measurement, 5.0, 1e-12);
  EXPECT_NE(sight.m_CalcStr.Find("Celestial azimuth line of position"),
            wxNOT_FOUND);
  EXPECT_NE(sight.m_CalcStr.Find("not a horizontal sextant angle"),
            wxNOT_FOUND);
}

TEST(AzimuthSight, MagneticModeExplainsDeviationBoundary) {
  wxDateTime utc;
  ASSERT_TRUE(utc.ParseDateTime("2026-08-14 12:00:00"));
  Sight sight(Sight::AZIMUTH, "Sun", Sight::CENTER, utc, 1.0, 90.0, 2.0);
  sight.m_bMagneticNorth = true;
  sight.Recompute(0);
  EXPECT_TRUE(sight.IsCalculated());
  EXPECT_NE(sight.m_CalcStr.Find("compass deviation already removed"),
            wxNOT_FOUND);
}
