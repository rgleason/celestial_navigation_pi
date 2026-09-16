// Opt-in bridge: test the headless adapter against the actual plugin path.
// This is not an external accuracy reference.
#define main engine_lab_cli_main
#include "production_runner.cpp"
#undef main
#include <gtest/gtest.h>
#include "Sight.h"

TEST(EngineLabAdapter, MatchesProductionDe440Samples) {
  for (const auto& timestamp : {"2024-06-13T19:26:00", "2025-03-05T15:00:00",
                                 "2024-06-30T23:59:59", "2024-02-29T00:00:00"}) {
    wxDateTime time;
    ASSERT_TRUE(time.ParseISOCombined(timestamp));
    Sight sight(Sight::LUNAR, "Sun", Sight::LUNAR_NEAR, time, 0,
                85.0 + 40.3 / 60.0, 0.2);
    sight.m_LunarMoonAltitude = 34 + 34.0 / 60;
    sight.m_LunarBodyAltitude = 51 + 58.0 / 60;
    sight.m_TimeCertainty = 120;
    sight.Recompute(0);
    Ephemeris headless(ECLIPSE_DE440_TEST_PATH, std::string(timestamp) + "Z");
    for (double offset : {-90.1234, 0.0, 0.123, 120.5678}) {
      lunar_distance::EphemerisSample actual;
      std::string error;
      ASSERT_TRUE(sight.LunarEphemeris()(offset, &actual, &error)) << error;
      const auto expected = headless.sample(offset);
      const double a[] = {actual.predicted_distance_deg, actual.moon_semidiameter_deg,
        actual.moon_horizontal_parallax_deg, actual.body_semidiameter_deg,
        actual.body_horizontal_parallax_deg, actual.moon_geographic_latitude_deg,
        actual.moon_geographic_longitude_deg, actual.body_geographic_latitude_deg,
        actual.body_geographic_longitude_deg};
      const double b[] = {expected.predicted_distance_deg, expected.moon_semidiameter_deg,
        expected.moon_horizontal_parallax_deg, expected.body_semidiameter_deg,
        expected.body_horizontal_parallax_deg, expected.moon_geographic_latitude_deg,
        expected.moon_geographic_longitude_deg, expected.body_geographic_latitude_deg,
        expected.body_geographic_longitude_deg};
      for (int i = 0; i < 9; ++i)
        EXPECT_NEAR(a[i], b[i], 1e-10) << timestamp << " offset " << offset << " field " << i;
      ASSERT_TRUE(actual.observer_direction);
      ASSERT_TRUE(expected.observer_direction);
      EXPECT_TRUE(actual.dut1_available);
      EXPECT_DOUBLE_EQ(actual.dut1_seconds,expected.dut1_seconds);
      for (const auto point : {lunar_distance::GeographicPoint(41.37,-71.48),
                               lunar_distance::GeographicPoint(-30,-60)}) {
        for (bool moon : {false,true}) {
          double aa,az,sd,bb,bz,bd;
          ASSERT_TRUE(actual.observer_direction(point.latitude_deg,point.longitude_deg,6.1,moon,&aa,&az,&sd));
          ASSERT_TRUE(expected.observer_direction(point.latitude_deg,point.longitude_deg,6.1,moon,&bb,&bz,&bd));
          EXPECT_NEAR(aa,bb,1e-10);
          EXPECT_NEAR(az,bz,1e-10);
          EXPECT_NEAR(sd,bd,1e-10);
        }
      }
    }
  }
}
