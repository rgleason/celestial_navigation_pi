#include <gtest/gtest.h>
#include "eclipse/dut1.h"
#include "eclipse/time.h"
#include "eclipse/astronomy.h"
#include "eclipse/spk.h"
#include "LunarDistanceEngine.h"
#include "Sight.h"
#include <cmath>
#include <limits>

TEST(Dut1, CoverageAndNoExtrapolation) {
  EXPECT_TRUE(eclipse::LookupDut1(eclipse::Dut1FirstUtcJd()).available);
  EXPECT_TRUE(eclipse::LookupDut1(eclipse::Dut1LastUtcJd()).available);
  EXPECT_FALSE(eclipse::LookupDut1(eclipse::Dut1FirstUtcJd()-0.1).available);
  const auto missing=eclipse::LookupDut1(eclipse::Dut1LastUtcJd()+0.1);
  EXPECT_FALSE(missing.available);
  EXPECT_DOUBLE_EQ(missing.seconds,0);
  EXPECT_FALSE(eclipse::LookupDut1(std::numeric_limits<double>::quiet_NaN()).available);
  EXPECT_EQ(eclipse::LookupDut1(eclipse::Dut1LastUtcJd()).quality,'P');
}

TEST(Dut1, LeapIsNotSmearedIntoPriorDay) {
  const double january=2457754.5;  // 2017-01-01 00:00 UTC
  const auto before=eclipse::LookupDut1(january-1.0/86400);
  const auto after=eclipse::LookupDut1(january);
  EXPECT_NEAR(before.seconds,-0.40878,0.001);
  EXPECT_NEAR(after.seconds,+0.59122,0.001);
  EXPECT_NEAR(after.seconds-before.seconds,1,0.000001);
  EXPECT_LT(eclipse::LookupDut1(january-0.5).seconds,-0.4);
}

TEST(Dut1, CompleteDailyDataAndFiniteInterpolation) {
  for (double day=eclipse::Dut1FirstUtcJd();day<eclipse::Dut1LastUtcJd();day+=1) {
    for (double fraction : {0.0,0.5,0.999}) {
      const auto value=eclipse::LookupDut1(day+fraction);
      ASSERT_TRUE(value.available) << day;
      ASSERT_TRUE(std::isfinite(value.seconds));
      ASSERT_LT(std::abs(value.seconds),0.95);
      ASSERT_TRUE(value.quality=='C' || value.quality=='I' || value.quality=='P');
    }
  }
}

TEST(LunarObserver, FailureDoesNotFallBackToOtherGeometry) {
  lunar_distance::Observation settings;
  settings.use_ellipsoid=true;
  settings.pressure_hpa=0;
  settings.artificial_horizon=true;
  auto ephemeris=[](double,lunar_distance::EphemerisSample* sample,std::string*) {
    *sample={};
    sample->observer_direction=[](double,double,double,bool,double*,double*,double*) {return false;};
    return true;
  };
  EXPECT_FALSE(lunar_distance::PredictTimeTaggedObservation(settings,ephemeris,0,{0,0}).valid);
}

TEST(LunarObserver, BadKernelAndCoordinatesFailWithoutThrowing) {
  eclipse::SpkKernel kernel;
  eclipse::EarthOrientation orientation;
  orientation.tt_jd=orientation.ut1_jd=2451545;
  double altitude,azimuth,sd;
  std::string error;
  EXPECT_FALSE(eclipse::ObserverApparentDirection(kernel,0,orientation,0,0,0,true,
                                                &altitude,&azimuth,&sd,&error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(eclipse::ObserverApparentDirection(kernel,0,orientation,91,0,0,true,
                                                &altitude,&azimuth,&sd,&error));
}

TEST(LunarObserver, FarFutureSightsStillSolveOfflineWithWarning) {
  // Self-consistency/availability checks, not independent future accuracy.
  for (int year : {2035,2075,2100,2140}) {
    wxDateTime time(13,wxDateTime::Jun,year,12,0,0);
    Sight sight(Sight::LUNAR,"Sun",Sight::LUNAR_NEAR,time,120,85,0.2);
    sight.m_LunarMoonAltitude=35;
    sight.m_LunarBodyAltitude=50;
    sight.Recompute(0);
    ASSERT_TRUE(sight.m_LunarUsesDe440) << year;
    ASSERT_TRUE(sight.m_LunarDut1Fallback) << year;
    EXPECT_TRUE(sight.m_CalcStr.Contains("UT1=UTC"));
    auto ephemeris=sight.LunarEphemeris();
    lunar_distance::EphemerisSample sample;
    std::string error;
    ASSERT_TRUE(ephemeris(0,&sample,&error)) << error;
    EXPECT_FALSE(sample.dut1_available);
    EXPECT_DOUBLE_EQ(sample.dut1_seconds,0);
    auto settings=sight.LunarObservation();
    settings.artificial_horizon=true;
    settings.pressure_hpa=0.000001;
    settings.index_error_arcmin=0;
    settings.eye_height_m=0;
    settings.moon_altitude_limb=settings.body_altitude_limb=lunar_distance::AltitudeLimb::Center;
    settings.moon_contact=settings.body_contact=lunar_distance::DistanceContact::Center;
    const double rad=std::acos(-1.0)/180;
    const double lon=std::atan2(std::sin(sample.moon_geographic_longitude_deg*rad)+std::sin(sample.body_geographic_longitude_deg*rad),
                               std::cos(sample.moon_geographic_longitude_deg*rad)+std::cos(sample.body_geographic_longitude_deg*rad))/rad;
    lunar_distance::GeographicPoint station(10+(sample.moon_geographic_latitude_deg+sample.body_geographic_latitude_deg)/2,lon);
    const auto generated=lunar_distance::PredictTimeTaggedObservation(settings,ephemeris,0,station);
    ASSERT_TRUE(generated.valid) << year << " " << generated.error;
    settings.raw_distance_deg=generated.raw_distance_deg;
    settings.moon_altitude_deg=generated.moon_altitude_deg;
    settings.body_altitude_deg=generated.body_altitude_deg;
    lunar_distance::SolveOptions options;
    options.start_offset_seconds=-30;
    options.end_offset_seconds=30;
    options.scan_step_seconds=10;
    const auto solved=lunar_distance::SolveTime(settings,ephemeris,options);
    ASSERT_TRUE(solved.valid) << year << " " << solved.error;
    bool recovered=false;
    for (const auto& candidate:solved.candidates)
      for (const auto& position:candidate.positions)
        if (std::abs(candidate.offset_seconds)<0.1 &&
            lunar_distance::GreatCircleDistanceNm(position,station)<0.01) recovered=true;
    EXPECT_TRUE(recovered) << year;
  }
}
