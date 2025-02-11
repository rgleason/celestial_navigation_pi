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
    // Common setup code
  }

  void TearDown() override {
    // Common cleanup code
  }
};

void SetMockPluginDataDir() {
  // Get the test data directory from CMake
  const char* datadir = TESTDATA;
  if (!datadir) {
    std::cout
        << "TESTDATA not defined in CMake, this is a build configuration error"
        << std::endl;
    return;
  }
  std::cout << "Using plugin data directory: " << datadir << std::endl;
}

TEST_F(AltitudeTest, NauticalAlmanacExample) {
  std::cout << "Starting NauticalAlmanacExample test..." << std::endl;

  SetMockPluginDataDir();

  std::cout << "Setting up test data..." << std::endl;
  // Example from Nautical Almanac 2024, page 277
  // Date: Jan 17, 2025
  // DR Position: Lat 40°N, Long 140°W
  // Time: 15:28:24 GMT
  // Body: Sun
  // Sextant altitude (Hs): 25°30.2'
  // Index error: -2.0'
  // Height of eye: 15 meters
  // Temperature: 10°C
  // Pressure: 1010 mb

  std::cout << "Creating datetime object..." << std::endl;
  wxDateTime datetime;
  if (!datetime.ParseDateTime("2025-01-17 15:28:24")) {
    std::cout << "Failed to parse datetime!" << std::endl;
  }

  std::cout << "Creating Sight object..." << std::endl;
  Sight sight(Sight::ALTITUDE, "Sun", Sight::LOWER, datetime,
              0,          // time certainty
              25.503333,  // 25°30.2' in decimal degrees
              1.0);       // measurement certainty 1'

  std::cout << "Setting environmental parameters..." << std::endl;
  // Set the environmental parameters using direct member access
  sight.m_IndexError = -2.0;
  sight.m_EyeHeight = 15;
  sight.m_Temperature = 10;
  sight.m_Pressure = 1010;

  std::cout << "Recomputing sight..." << std::endl;
  // Recompute the sight
  sight.Recompute(0);

  // From Nautical Almanac example, expected Hc = 25°24.8'
  const double expected_hc = 25.413333;  // 25°24.8' in decimal degrees
  const double epsilon = 0.1 / 60.0;     // 0.1 arc-minutes tolerance

  std::cout << "Getting calculated altitude..." << std::endl;
  // Get calculated altitude from m_ObservedAltitude
  double calculated_hc = sight.m_ObservedAltitude;

  EXPECT_NEAR(calculated_hc, expected_hc, epsilon)
      << "Expected Hc: " << expected_hc << "°\n"
      << "Calculated Hc: " << calculated_hc << "°\n"
      << "Difference: " << (calculated_hc - expected_hc) * 60 << "'";
}