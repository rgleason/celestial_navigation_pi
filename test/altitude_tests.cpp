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
#include "../src/Sight.h"
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

TEST_F(AltitudeTest, NauticalAlmanacExample) {
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
    
    wxDateTime datetime;
    datetime.ParseDateTime("2025-01-17 15:28:24");
    
    Sight sight(Sight::ALTITUDE, "Sun", Sight::LOWER, datetime,
                0,  // time certainty
                25.503333,  // 25°30.2' in decimal degrees
                1.0);  // measurement certainty 1'
    
    // Set the environmental parameters
    sight.SetIndexError(-2.0);
    sight.SetEyeHeight(15);
    sight.SetTemperature(10);
    sight.SetPressure(1010);
    
    // Recompute the sight
    sight.Recompute(0);
    
    // From Nautical Almanac example, expected Hc = 25°24.8'
    const double expected_hc = 25.413333;  // 25°24.8' in decimal degrees
    const double epsilon = 0.1/60.0;  // 0.1 arc-minutes tolerance
    
    double calculated_hc = sight.GetCalculatedAltitude();
    
    EXPECT_NEAR(calculated_hc, expected_hc, epsilon)
        << "Expected Hc: " << expected_hc << "°\n"
        << "Calculated Hc: " << calculated_hc << "°\n"
        << "Difference: " << (calculated_hc - expected_hc) * 60 << "'";
}