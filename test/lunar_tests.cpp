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
static int error_lonerror;
static int error_poserror;

static void commontest(Sight &sight, wxDateTime &datetime,
                       const char *body, int line, const char *date,
                       double dr_lat, double dr_lon,
                       double expected_ldc, long expected_timechange,
                       double expected_lonerror, double expected_poserror,
                       int epsilon) {

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
    double dr_lat;
    double dr_lon;
    double expected_ldc;
    long expected_timechange;
    double expected_lonerror;
    double expected_poserror;
};

struct lunarsightdata LUNAR_SIGHTS[] = {
    { "2025-08-18 11:58:00", "Sun", Sight::NEAR, 17, 1013, -0.8, 2.4, { 59, 18.8 }, Sight::LOWER, { 70, 4 }, Sight::LOWER, { 17, 1}, 43.2676, -76.9798, 60.104126, -91, -22.74, -16.55836 },
};

TEST_F(LunarTest, Sight) {
    std::vector<int> lunar_ldc;
    std::vector<int> lunar_timechange;
    std::vector<int> lunar_lonerror;
    std::vector<int> lunar_poserror;

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

        const int EPSILON = 1;  // 0.1 arc-minute tolerance
        commontest(sight, datetime, data.body, i, data.date, data.dr_lat, data.dr_lon,
                   data.expected_ldc, data.expected_timechange,
                   data.expected_lonerror, data.expected_poserror,
                   EPSILON);

        lunar_ldc.push_back(error_ldc);
        lunar_timechange.push_back(error_timechange);
        lunar_lonerror.push_back(error_lonerror);
        lunar_poserror.push_back(error_poserror);
    }

    std::cout << "============================================================================="
              << std::endl;
    report("Lunar", "LDC", lunar_ldc);
    report("Lunar", "Time change", lunar_timechange);
    report("Lunar", "Lon error", lunar_lonerror);
    report("Lunar", "Pos error", lunar_poserror);
    std::cout << "============================================================================="
              << std::endl;
}
