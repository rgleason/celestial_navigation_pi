#include <gtest/gtest.h>

#include "eclipse/astronomy.h"
#include "eclipse/spk.h"
#include "eclipse/time.h"

#include <algorithm>
#include <cmath>

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
