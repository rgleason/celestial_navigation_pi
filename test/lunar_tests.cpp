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
#include "common.h"

class LunarTest : public ::testing::Test {
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

static int error_ldc;
static int error_timechange;

static void commontest(Sight &sight, wxDateTime &datetime,
                       const char *body, int line, const char *date,
                       double expected_ldc, long expected_timechange,
                       double epsilon) {

    sight.Recompute(0);  // 0 = no clock offset

    std::cout << "=== " << body << " (line " << line << ", date " << date << ") ==="
              << std::endl << std::endl;

    // Test LDC
    error_ldc = sight.m_LDC - expected_ldc;
    EXPECT_NEAR(sight.m_LDC, expected_ldc, epsilon)
        << "LDC differs from test sample by "
        << error_ldc << std::endl;

    // Test timechange
    error_timechange = sight.m_TimeCorrection - expected_timechange;
    EXPECT_NEAR(sight.m_TimeCorrection, expected_timechange, epsilon)
        << "Time correction differs from test sample by "
        << error_timechange << std::endl;

    // Print calculation string
    std::cout << "Detailed Calculation String:" << std::endl;
    std::cout << sight.m_CalcStr << std::endl;
}

struct lunarsightdata {
    const char *date;
    const char *body;
    Sight::BodyLimb limb;
    double temp;
    double pres;
    double ie;
    double eye;
    struct degmin ld;
    Sight::BodyLimb limb_moon;
    struct degmin hs_moon;
    Sight::BodyLimb limb_body;
    struct degmin hs_body;
    double expected_ldc;
    long expected_timechange;
};

struct lunarsightdata LUNAR_SIGHTS[] = {
    /* case 1 */
    { "2025-08-18 11:58:00", "Sun", Sight::LUNAR_NEAR, 17, 1013, -0.8, 2.4, { 59, 18.8 }, Sight::LOWER, { 70, 4 }, Sight::LOWER, { 17, 1}, 60.104126, -91 },
    /* case 2 */
    { "2025-08-18 11:58:00", "Sun", Sight::LUNAR_NEAR, 17, 1013, -0.8, 2.4, { 59, 18.0 }, Sight::LOWER, { 70, 4 }, Sight::LOWER, { 17, 1}, 60.091036, -4 },
    /* case 5 */
//    { "2025-08-09 07:00:00", "Saturn", Sight::LUNAR_NEAR, 10, 1010, 0.1, 2.4, { 45, 2.8 }, Sight::LOWER, { 22, 53 }, Sight::CENTER, { 42, 11 }, 44.800810, -3 },
    /* case 6 */
    { "2025-08-09 07:00:00", "Saturn", Sight::LUNAR_NEAR, 10, 1010, 0.1, 2.4, { 44, 56.5 }, Sight::LOWER, { 63, 59 }, Sight::CENTER, { 70, 38 }, 44.800810, -3 },
};

TEST_F(LunarTest, Sight) {
    std::vector<int> lunar_ldc;
    std::vector<int> lunar_timechange;

    int count = (int)(sizeof(LUNAR_SIGHTS) / sizeof(LUNAR_SIGHTS[0]));
    for (int i = 0; i < count; i++) {
        struct lunarsightdata data = LUNAR_SIGHTS[i];

        wxDateTime datetime;
        ASSERT_TRUE(datetime.ParseDateTime(data.date)) << "Failed to parse datetime";

        Sight sight(Sight::LUNAR, data.body, data.limb, datetime, 0,
                    DegMin2DecDeg(data.ld.deg, data.ld.min), 1);
        sight.m_IndexError = data.ie;
        sight.m_EyeHeight = data.eye;
        sight.m_Temperature = data.temp;
        sight.m_Pressure = data.pres;
        sight.m_LunarMoonLimb = data.limb_moon;
        sight.m_LunarMoonAltitude = DegMin2DecDeg(data.hs_moon.deg, data.hs_moon.min);
        sight.m_LunarBodyLimb = data.limb_body;
        sight.m_LunarBodyAltitude = DegMin2DecDeg(data.hs_body.deg, data.hs_body.min);
        sight.m_TimeCertainty = 10800;

        const double EPSILON = 0.001;
        commontest(sight, datetime, data.body, i, data.date,
                   data.expected_ldc, data.expected_timechange,
                   EPSILON);

        lunar_ldc.push_back(error_ldc);
        lunar_timechange.push_back(error_timechange);
    }

    std::cout << "============================================================================="
              << std::endl;
    report("Lunar", "LDC", lunar_ldc);
    report("Lunar", "Time change", lunar_timechange);
    std::cout << "============================================================================="
              << std::endl;
}
