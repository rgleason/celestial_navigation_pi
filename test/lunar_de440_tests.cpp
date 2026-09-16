#include <gtest/gtest.h>

#include "eclipse/astronomy.h"
#include "eclipse/spk.h"
#include "eclipse/time.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include "Sight.h"
#include "UtcDateTime.h"
extern "C" {
#include "erfa.h"
}

TEST(LunarDe440, PointJudithApparentDirectionsMatchHorizons) {
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;
  eclipse::CalendarDateTime utc;
  utc.year = 2024;
  utc.month = 6;
  utc.day = 13;
  utc.hour = 19;
  utc.minute = 26;
  double jd;
  ASSERT_TRUE(eclipse::CalendarToJulianDate(utc, &jd, &error));
  const double tt = jd + (37.0 + 32.184) / 86400.0;
  const double et =
      (tt - 2451545.0) * 86400.0 + eclipse::TdbMinusTtSeconds(tt, jd);
  // Independent JPL Horizons DE441 observer output, geocentre, quantities 2,
  // airless apparent RA/Dec of date, downloaded 2026-09-09. Tolerance 0.1"
  // covers the small solar light-deflection term omitted here.
  const int ids[] = {301, 10};
  const double ra[] = {170.510425856, 82.661667343};
  const double dec[] = {6.344322280, 23.266996432};
  double matrix[3][3];
  eraPnm06a(2451545.0, tt - 2451545.0, matrix);
  for (int i = 0; i < 2; ++i) {
    eclipse::Vector3 v;
    ASSERT_TRUE(
        eclipse::ApparentGeocentricPosition(kernel, ids[i], et, &v, &error));
    double icrf[] = {v.x, v.y, v.z}, apparent[3];
    eraRxp(matrix, icrf, apparent);
    const double alpha = atan2(apparent[1], apparent[0]) * 180.0 / M_PI;
    const double delta =
        atan2(apparent[2], hypot(apparent[0], apparent[1])) * 180.0 / M_PI;
    EXPECT_NEAR(alpha, ra[i], 0.1 / 3600.0);
    EXPECT_NEAR(delta, dec[i], 0.1 / 3600.0);
  }
}

static Sight PointJudith() {
  wxDateTime time;
  time.ParseISOCombined("2024-06-13T19:26:00");
  Sight sight(Sight::LUNAR, "Sun", Sight::LUNAR_NEAR, time, 0,
              85.0 + 40.3 / 60.0, 0.2);
  sight.m_EyeHeight = 6.1;
  sight.m_IndexError = 0.0;
  sight.m_Temperature = 23.9;
  sight.m_Pressure = 1019.3;
  sight.m_LunarMoonAltitude = 34.0 + 34.0 / 60.0;
  sight.m_LunarBodyAltitude = 51.0 + 58.0 / 60.0;
  sight.m_LunarMoonLimb = Sight::UPPER;
  sight.m_LunarBodyLimb = Sight::LOWER;
  sight.m_LunarBodyDistanceLimb = Sight::LUNAR_NEAR;
  sight.m_TimeCertainty = 7200;
  sight.m_DRLat = 41.0 + 22.0 / 60.0;
  sight.m_DRLon = -(71.0 + 29.0 / 60.0);
  return sight;
}

TEST(LunarDe440, PointJudithReferenceAndCoincidentTimeModes) {
  Sight sight = PointJudith();
  sight.Recompute(0);
  ASSERT_TRUE(sight.m_LunarSolutionValid) << sight.m_CalcStr;
  ASSERT_FALSE(sight.m_LunarCandidates.empty());
  auto direct = sight.m_LunarCandidates.front();
  double nearest = INFINITY;
  for (const auto& candidate : sight.m_LunarCandidates) {
    for (const auto& position : candidate.positions) {
      const double distance = lunar_distance::GreatCircleDistanceNm(
          {sight.m_DRLat, sight.m_DRLon}, position);
      if (distance < nearest) {
        nearest = distance;
        direct = candidate;
      }
      std::cout << "WGS84 branch: " << candidate.offset_seconds << " s at "
                << position.latitude_deg << ", " << position.longitude_deg
                << std::endl;
    }
  }
  std::cout << "Point Judith: LDc=" << std::setprecision(12) << sight.m_LDC
            << " correction=" << direct.offset_seconds
            << " sigma=" << direct.time_uncertainty_seconds << std::endl;
  // Reed's printed cleared LD is rounded to 0.1 arcminute. Do not require
  // a rounded external answer to match an unrounded computation exactly.
  EXPECT_NEAR(sight.m_LDC, 85.0 + 31.8 / 60.0, 0.05 / 60.0);
  lunar_distance::EphemerisSample sample;
  std::string error;
  ASSERT_TRUE(sight.LunarEphemeris()(0, &sample, &error));
  const auto cleared =
      lunar_distance::ClearDistance(sight.LunarObservation(), sample);
  std::cout << "pred=" << sample.predicted_distance_deg
            << " HP=" << sample.moon_horizontal_parallax_deg * 60
            << " SD=" << sample.moon_semidiameter_deg * 60
            << " app=" << cleared.moon_apparent_center_altitude_deg << ","
            << cleared.body_apparent_center_altitude_deg
            << " Ho=" << cleared.moon_geocentric_altitude_deg << ","
            << cleared.body_geocentric_altitude_deg
            << " R=" << cleared.moon_refraction_deg * 60 << ","
            << cleared.body_refraction_deg * 60 << std::endl;
  auto rad = [](double a) { return a * M_PI / 180.0; };
  double from_gps = acos(sin(rad(sample.moon_geographic_latitude_deg)) *
                             sin(rad(sample.body_geographic_latitude_deg)) +
                         cos(rad(sample.moon_geographic_latitude_deg)) *
                             cos(rad(sample.body_geographic_latitude_deg)) *
                             cos(rad(sample.moon_geographic_longitude_deg -
                                     sample.body_geographic_longitude_deg))) *
                    180.0 / M_PI;
  EXPECT_NEAR(from_gps, sample.predicted_distance_deg, 1e-10);
  const auto fixed_utc = lunar_distance::PositionAtTime(
      sight.LunarObservation(), sight.LunarEphemeris(), 0);
  ASSERT_TRUE(fixed_utc.valid) << fixed_utc.error;
  double fixed_utc_nearest = INFINITY;
  for (const auto& p : fixed_utc.candidates) {
    const double distance = lunar_distance::GreatCircleDistanceNm(
        {sight.m_DRLat, sight.m_DRLon}, p);
    if (distance < fixed_utc_nearest) fixed_utc_nearest = distance;
    std::cout << "Entered-UTC branch: " << p.latitude_deg << ", "
              << p.longitude_deg << std::endl;
  }
  EXPECT_LT(fixed_utc_nearest, 0.5);  // compare the reference at its stated UTC
  auto observation = sight.LunarObservation();
  observation.separate_times = true;
  lunar_distance::SolveOptions options;
  options.start_offset_seconds = -60;
  options.end_offset_seconds = 60;
  options.scan_step_seconds = 10;
  const auto tagged = lunar_distance::SolveTimeTagged(
      observation, sight.LunarEphemeris(), options);
  ASSERT_TRUE(tagged.valid) << tagged.error;
  ASSERT_EQ(sight.m_LunarCandidates.size(), tagged.candidates.size());
  for (std::size_t i = 0; i < tagged.candidates.size(); ++i)
    EXPECT_NEAR(tagged.candidates[i].offset_seconds,
                sight.m_LunarCandidates[i].offset_seconds, 0.15);
}

TEST(LunarDe440, Wgs84AirlessMoonAltitudeMatchesHorizons) {
  Sight sight = PointJudith();
  sight.Recompute(0);
  auto settings = sight.LunarObservation();
  settings.pressure_hpa = 0.0;
  settings.index_error_arcmin = 0.0;
  settings.artificial_horizon =
      true;  // no dip; measured angle is twice altitude
  settings.moon_altitude_limb = lunar_distance::AltitudeLimb::Center;
  settings.body_altitude_limb = lunar_distance::AltitudeLimb::Center;
  auto prediction = lunar_distance::PredictTimeTaggedObservation(
      settings, sight.LunarEphemeris(), 0, {sight.m_DRLat, sight.m_DRLon});
  ASSERT_TRUE(prediction.valid) << prediction.error;
  // JPL Horizons DE441, geodetic site (-71.4833333333,41.3666666667,0.0061km),
  // 2024-06-13 19:26 UTC, quantities 4/13, APPARENT=AIRLESS, queried
  // 2026-09-09. Allow omitted diurnal aberration, measured DUT1 and polar
  // motion (<1 arcsec).
  EXPECT_NEAR(prediction.moon_altitude_deg / 2.0, 34.221386789, 1.0 / 3600.0);
  settings.use_ellipsoid = false;
  const auto sphere = lunar_distance::PredictTimeTaggedObservation(
      settings, sight.LunarEphemeris(), 0, {sight.m_DRLat, sight.m_DRLon});
  EXPECT_GT(std::fabs(sphere.moon_altitude_deg / 2.0 - 34.221386789),
            2.0 / 3600.0);
}

TEST(LunarDe440, Wgs84RecoversSyntheticLimbsContactsAndMovingReadingTimes) {
  Sight sight = PointJudith();
  sight.Recompute(0);
  for (bool separate : {false, true}) {
    for (int limb = 0; limb < 3; ++limb) {
      auto settings = sight.LunarObservation();
      settings.separate_times = separate;
      settings.moon_time_offset_seconds = separate ? -90 : 0;
      settings.body_time_offset_seconds = separate ? 120 : 0;
      settings.moving_observer = separate;
      settings.course_true_deg = 125;
      settings.speed_knots = 8;
      settings.moon_altitude_limb =
          static_cast<lunar_distance::AltitudeLimb>(limb);
      settings.body_altitude_limb =
          static_cast<lunar_distance::AltitudeLimb>(2 - limb);
      settings.moon_contact =
          static_cast<lunar_distance::DistanceContact>(limb);
      settings.body_contact =
          static_cast<lunar_distance::DistanceContact>(2 - limb);
      const lunar_distance::GeographicPoint truth(sight.m_DRLat, sight.m_DRLon);
      const auto predicted = lunar_distance::PredictTimeTaggedObservation(
          settings, sight.LunarEphemeris(), 23.75, truth);
      ASSERT_TRUE(predicted.valid);
      settings.raw_distance_deg = predicted.raw_distance_deg;
      settings.moon_altitude_deg = predicted.moon_altitude_deg;
      settings.body_altitude_deg = predicted.body_altitude_deg;
      lunar_distance::SolveOptions options;
      options.start_offset_seconds = -60;
      options.end_offset_seconds = 60;
      options.scan_step_seconds = 10;
      const auto recovered =
          separate ? lunar_distance::SolveTimeTagged(
                         settings, sight.LunarEphemeris(), options)
                   : lunar_distance::SolveTime(settings, sight.LunarEphemeris(),
                                               options);
      ASSERT_TRUE(recovered.valid) << recovered.error;
      double best = INFINITY, time_error = INFINITY;
      for (const auto& candidate : recovered.candidates)
        for (const auto& position : candidate.positions) {
          double distance =
              lunar_distance::GreatCircleDistanceNm(position, truth);
          if (distance < best) {
            best = distance;
            time_error = std::fabs(candidate.offset_seconds - 23.75);
          }
        }
      EXPECT_LT(best, 0.03);
      EXPECT_LT(time_error, 0.1);
    }
  }
}

TEST(LunarDe440, MatchesIndependentJplHorizonsSunMoonSeparation) {
  eclipse::SpkKernel kernel;
  std::string error;
  ASSERT_TRUE(kernel.Open(ECLIPSE_DE440_TEST_PATH, &error)) << error;

  eclipse::CalendarDateTime utc;
  utc.year = 2025;
  utc.month = 8;
  utc.day = 18;
  utc.hour = 11;
  utc.minute = 58;
  utc.second = 0.0;
  double utc_jd = 0.0;
  ASSERT_TRUE(eclipse::CalendarToJulianDate(utc, &utc_jd, &error)) << error;
  EXPECT_DOUBLE_EQ(eclipse::TaiMinusUtcSeconds(utc), 37.0);
  const double tt_jd =
      utc_jd + (eclipse::TaiMinusUtcSeconds(utc) + 32.184) / 86400.0;
  const double tdb_jd =
      tt_jd + eclipse::TdbMinusTtSeconds(tt_jd, utc_jd) / 86400.0;
  const double et = (tdb_jd - 2451545.0) * 86400.0;

  eclipse::Vector3 moon;
  eclipse::Vector3 sun;
  ASSERT_TRUE(eclipse::AstrometricPosition(kernel, 301, 399, et, &moon,
                                           &error))
      << error;
  ASSERT_TRUE(eclipse::AstrometricPosition(kernel, 10, 399, et, &sun,
                                           &error))
      << error;
  const double cosine = eclipse::Dot(moon, sun) / (moon.Norm() * sun.Norm());
  const double separation =
      std::acos(std::max(-1.0, std::min(1.0, cosine))) * 180.0 / M_PI;

  // Independent NASA/JPL Horizons observer tables (DE441, geocentric,
  // astrometric ICRF) give Moon 05:39:32.77 +28:32:58.6 and Sun
  // 09:50:51.83 +13:02:11.2, whose rounded-coordinate separation is
  // 60.0938 degrees. DE440s must agree within the Horizons print precision.
  EXPECT_NEAR(separation, 60.0938, 0.001);
}
