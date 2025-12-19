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

// Helper functions
double DegMin2DecDeg(double degrees, double minutes) {
    double result = std::abs(degrees) + (std::abs(minutes) / 60.0);
    return (degrees < 0 || minutes < 0) ? -result : result;
}

std::string TenthsMinutestoStr(int tenths_minutes) {
    int degrees = tenths_minutes / 600;
    double minutes = ((double)(tenths_minutes - degrees * 600)) / 10;
    std::stringstream ss;
    ss << degrees << "° " << std::fixed << std::setprecision(1) << minutes << "'";
    return ss.str();
}

int DecDegToTenthsMinutes(double decimal_degrees) {
    return round(decimal_degrees * 60 * 10);
}

void report(const char *name, const char *type, std::vector<int> &vec) {

    double mean = 0;
    for (int i = 0; i < (int) vec.size(); i++) {
        mean += vec[i];
    }
    mean /= vec.size() * 10;

    double deviation = 0;
    for (int i = 0; i < (int) vec.size(); i++) {
        deviation += (((double)vec[i]) / 10.0  - mean) * (((double)vec[i] / 10.0) - mean);
    }
    deviation /= (vec.size() - 1);
    deviation = sqrt(deviation);

    double min = DBL_MAX;
    double max = 0;
    int exact = 0;
    int overone = 0;
    int underone = 0;
    for (int i = 0; i < (int) vec.size(); i++) {
        if (vec[i] == 0)
            ++exact;
        if (vec[i] < -1)
            ++underone;
        if (vec[i] > 1)
            ++overone;
        double value = (double)vec[i] / 10.0;
        if (value < min)
            min = value;
        if (value > max)
            max = value;
    }

    std::cout << name << " " << type << ":" << std::endl;
    std::cout << "    number of values: " << vec.size() << std::endl;
    std::cout << "    min difference: " << min << std::endl;
    std::cout << "    max difference: " << max << std::endl;
    std::cout << "    mean difference: " << mean << std::endl;
    std::cout << "    standard deviation: " << deviation << std::endl;
    std::cout << "    exact matches: " << exact
              << " (" << ((exact * 100) / vec.size())<< "%)" << std::endl;
    std::cout << "    under -0.1' matches: " << underone
              << " (" << ((underone * 100) / vec.size()) << "%)" << std::endl;
    std::cout << "    over 0.1' matches: " << overone
              << " (" << ((overone * 100) / vec.size()) << "%)" << std::endl;
}


