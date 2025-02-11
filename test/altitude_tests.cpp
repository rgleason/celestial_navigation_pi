/***************************************************************************
 *   Copyright (C) 2024 by OpenCPN development team                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
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
#include <iomanip>
#include <sstream>

class AltitudeTest : public ::testing::Test {
protected:
  void SetUp() override {
    const char* datadir = TESTDATA;
    if (!datadir) {
      std::cout << "TESTDATA not defined in CMake" << std::endl;
      return;
    }
    std::cout << "Using plugin data directory: " << datadir << std::endl;
  }
};

// Helper functions
double DegMin2DecDeg(double degrees, double minutes) {
  return degrees + (minutes / 60.0);
}

std::string DecDegToDegMin(double decimal_degrees) {
  int degrees = static_cast<int>(decimal_degrees);
  double minutes = (decimal_degrees - degrees) * 60.0;
  std::stringstream ss;
  ss << degrees << "° " << std::fixed << std::setprecision(1) << minutes << "'";
  return ss.str();
}

TEST_F(AltitudeTest, SunLowerLimbExample) {
  // Test data from Example 1: Sun LL, Nov 13, 2024 at UT 20-17-45
  wxDateTime datetime;
  ASSERT_TRUE(datetime.ParseDateTime("2024-11-13 20:17:45"))
      << "Failed to parse datetime";

  // Input parameters
  const double SIGHT_HS = DegMin2DecDeg(11, 25.0);  // Hs (sight): 11°25.0'

  // Create sight object with known coordinates
  Sight sight(Sight::ALTITUDE,  // Type
              "Sun",            // Body
              Sight::LOWER,     // Using lower limb
              datetime,         // Time of sight
              0.0,              // Time certainty
              SIGHT_HS,         // Measured altitude
              1.0               // Measurement certainty
  );

  // Set environmental parameters
  sight.m_IndexError = 3.2;    // IE: +3.2'
  sight.m_EyeHeight = 2.4;     // Height of Eye: 2.4 meters
  sight.m_Temperature = 15.0;  // Temperature: 15°C
  sight.m_Pressure = 1013.0;   // Pressure: 1013 mb

  // Expected values from Nautical Almanac (NA)
  const double NA_MAIN_CORRECTION = DegMin2DecDeg(0, 11.6);  // 11.6'
  const double NA_HO = DegMin2DecDeg(11, 30.6);              // Ho: 11°30.6'
  const double NA_GHA = DegMin2DecDeg(128, 20.5);            // GHA: 128°20.5'
  const double NA_DEC = -DegMin2DecDeg(18, 15.2);            // Dec: S18°15.2'

  // Tolerances
  const double EPSILON_MIN = 0.1 / 60.0;  // 0.1 arc-minute tolerance
  const double EPSILON_DEG = 0.1;         // 0.1 degree tolerance

  // Test expectations
  std::cout << "\n=== Sun Lower Limb Example (Nov 13, 2024) ===" << std::endl;

  // 1. Test Main Correction (difference between Ho and Hs)
  double actualCorrection = sight.m_ObservedAltitude - SIGHT_HS;
  std::cout << "Main Correction Analysis:" << std::endl;
  std::cout << "  Almanac: " << (NA_MAIN_CORRECTION * 60.0) << "'" << std::endl;
  std::cout << "  Actual : " << (actualCorrection * 60.0) << "'" << std::endl;

  EXPECT_NEAR(actualCorrection, NA_MAIN_CORRECTION, EPSILON_MIN)
      << "Main correction differs from NA by "
      << ((actualCorrection - NA_MAIN_CORRECTION) * 60.0) << " minutes";

  // 2. Test Ho (Observed Altitude)
  std::cout << "\nHo Analysis:" << std::endl;
  std::cout << "  Almanac: " << DecDegToDegMin(NA_HO) << std::endl;
  std::cout << "  Actual : " << DecDegToDegMin(sight.m_ObservedAltitude)
            << std::endl;

  EXPECT_NEAR(sight.m_ObservedAltitude, NA_HO, EPSILON_MIN)
      << "Ho differs from NA by " << ((sight.m_ObservedAltitude - NA_HO) * 60.0)
      << " minutes";

  // 3. Test Body Position (GHA, Dec)
  double gha, dec;
  sight.BodyLocation(datetime, &dec, &gha, nullptr, nullptr);

  std::cout << "\nBody Position Analysis:" << std::endl;
  std::cout << "  Almanac GHA: " << DecDegToDegMin(NA_GHA)
            << ", Dec: " << DecDegToDegMin(std::abs(NA_DEC)) << " S"
            << std::endl;
  std::cout << "  Actual GHA : " << DecDegToDegMin(gha)
            << ", Dec: " << DecDegToDegMin(std::abs(dec))
            << (dec < 0 ? " S" : " N") << std::endl;

  EXPECT_NEAR(gha, NA_GHA, EPSILON_MIN)
      << "GHA differs from Almanac by " << ((gha - NA_GHA) * 60.0)
      << " minutes";
  EXPECT_NEAR(dec, NA_DEC, EPSILON_MIN)
      << "Declination differs from Almanac by " << ((dec - NA_DEC) * 60.0)
      << " minutes";

  // Print calculation string
  std::cout << "\nDetailed Calculation String:" << std::endl;
  std::cout << sight.m_CalcStr << std::endl;
}
