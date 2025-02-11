/***************************************************************************
 *   Copyright (C) 2024 by OpenCPN development team                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // precompiled headers
#include "wx/tokenzr.h"

#include <gtest/gtest.h>
#include "ocpn_plugin.h"
#include "Sight.h"
#include <cmath>

class AltitudeTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Get the test data directory from CMake
    const char* datadir = TESTDATA;
    if (!datadir) {
      std::cout << "TESTDATA not defined in CMake, this is a build "
                   "configuration error"
                << std::endl;
      return;
    }
    std::cout << "Using plugin data directory: " << datadir << std::endl;
  }

  void TearDown() override {}
};

// Converts degrees and minutes to decimal degrees
double DegMin2DecDeg(double degrees, double minutes) {
  return degrees + (minutes / 60.0);
}

TEST_F(AltitudeTest, AlmanacJan132024_1200GMT) {
  std::cout << "Starting AlmanacJan132024_1200GMT test..." << std::endl;

  // From Almanac 2024, January 13, 12:00 GMT
  // GHA = 357°52.6'
  // Dec = S21°30.9' (South is negative)
  // SD = 16.30'
  // d correction = 0.40

  wxDateTime datetime;
  if (!datetime.ParseDateTime("2024-01-13 12:00:00")) {
    std::cout << "Failed to parse datetime!" << std::endl;
    FAIL() << "Could not parse datetime";
  }

  std::cout << "Creating Sight object..." << std::endl;

  // Constants from the almanac
  const double ALMANAC_GHA = DegMin2DecDeg(357, 52.6);  // 357°52.6'
  const double ALMANAC_DEC =
      -DegMin2DecDeg(21, 30.9);            // S21°30.9' (negative for South)
  const double ALMANAC_SD = 16.30 / 60.0;  // 16.30' converted to degrees

  // Example position: Let's use latitude 45°N for this test
  const double OBSERVER_LAT = 45.0;

  // Calculate altitude we should see from this position
  // TODO: Add the actual calculation here
  const double CALCULATED_HS = 0.0;  // This needs to be calculated

  Sight sight(Sight::ALTITUDE,  // Type of sight
              "Sun",            // Celestial body
              Sight::LOWER,     // Using lower limb
              datetime,         // Time of sight
              0.0,              // Time certainty
              CALCULATED_HS,    // Measured altitude (will calculate)
              1.0);             // Measurement certainty 1'

  std::cout << "Setting environmental parameters..." << std::endl;
  sight.m_IndexError = 0.0;    // Assuming perfect index for this test
  sight.m_EyeHeight = 0.0;     // Assuming observations at sea level
  sight.m_Temperature = 10.0;  // Standard temperature
  sight.m_Pressure = 1010.0;   // Standard pressure

  std::cout << "Recomputing sight..." << std::endl;
  const int NO_CLOCK_OFFSET = 0;
  sight.Recompute(NO_CLOCK_OFFSET);

  // Compare with almanac values
  double gha, dec;
  sight.BodyLocation(datetime, &dec, &gha, nullptr, nullptr);

  const double EPSILON_DEG = 0.1 / 60.0;  // 0.1 arc-minute tolerance

  EXPECT_NEAR(gha, ALMANAC_GHA, EPSILON_DEG)
      << "GHA differs from almanac by: " << (gha - ALMANAC_GHA) * 60.0
      << " minutes";

  EXPECT_NEAR(dec, ALMANAC_DEC, EPSILON_DEG)
      << "Declination differs from almanac by: " << (dec - ALMANAC_DEC) * 60.0
      << " minutes";
}