#include "eclipse/time.h"

extern "C" {
#include "erfa.h"
}

#include <cmath>
#include <sstream>

namespace eclipse {

bool CalendarToJulianDate(const CalendarDateTime& calendar, double* jd,
                          std::string* error) {
  if (!jd) return false;
  if (calendar.hour < 0 || calendar.hour > 23 || calendar.minute < 0 ||
      calendar.minute > 59 || calendar.second < 0.0 ||
      calendar.second >= 60.0) {
    if (error) *error = "Invalid Gregorian calendar date/time";
    return false;
  }
  double first = 0.0;
  double second = 0.0;
  if (eraCal2jd(calendar.year, calendar.month, calendar.day, &first, &second) !=
      0) {
    if (error) *error = "Invalid Gregorian calendar date";
    return false;
  }
  const double seconds =
      calendar.hour * 3600.0 + calendar.minute * 60.0 + calendar.second;
  *jd = first + second + seconds / 86400.0;
  return true;
}

CalendarDateTime JulianDateToCalendar(double jd) {
  CalendarDateTime result;
  double fraction = 0.0;
  if (eraJd2cal(2451545.0, jd - 2451545.0, &result.year, &result.month,
                &result.day, &fraction) == 0) {
    double seconds = std::round(fraction * 86400.0 * 1e6) / 1e6;
    if (seconds >= 86400.0) seconds = 86399.999999;
    result.hour = static_cast<int>(seconds / 3600.0);
    seconds -= result.hour * 3600.0;
    result.minute = static_cast<int>(seconds / 60.0);
    result.second = seconds - result.minute * 60.0;
    if (result.second >= 59.9995) {
      result.second = 0.0;
      ++result.minute;
      if (result.minute >= 60) {
        result.minute = 0;
        ++result.hour;
      }
    }
  }
  return result;
}

double DecimalYear(const CalendarDateTime& calendar) {
  const bool leap = (calendar.year % 4 == 0 && calendar.year % 100 != 0) ||
                    calendar.year % 400 == 0;
  static const int cumulative_days[12] = {0,   31,  59,  90,  120, 151,
                                          181, 212, 243, 273, 304, 334};
  int day = cumulative_days[calendar.month - 1] + calendar.day - 1;
  if (leap && calendar.month > 2) ++day;
  const double fraction =
      (calendar.hour + (calendar.minute + calendar.second / 60.0) / 60.0) /
      24.0;
  return calendar.year + (day + fraction) / (leap ? 366.0 : 365.0);
}

double ModelDeltaTSeconds(double year) {
  double t = 0.0;
  if (year < 1800.0) {
    t = (year - 1820.0) / 100.0;
    return -20.0 + 32.0 * t * t;
  }
  if (year < 1860.0) {
    t = year - 1800.0;
    return 13.72 - 0.332447 * t + 0.0068612 * t * t +
           0.0041116 * std::pow(t, 3) - 0.00037436 * std::pow(t, 4) +
           0.0000121272 * std::pow(t, 5) - 0.0000001699 * std::pow(t, 6) +
           0.000000000875 * std::pow(t, 7);
  }
  if (year < 1900.0) {
    t = year - 1860.0;
    return 7.62 + 0.5737 * t - 0.251754 * t * t + 0.01680668 * std::pow(t, 3) -
           0.0004473624 * std::pow(t, 4) + std::pow(t, 5) / 233174.0;
  }
  if (year < 1920.0) {
    t = year - 1900.0;
    return -2.79 + 1.494119 * t - 0.0598939 * t * t + 0.0061966 * t * t * t -
           0.000197 * t * t * t * t;
  }
  if (year < 1941.0) {
    t = year - 1920.0;
    return 21.20 + 0.84493 * t - 0.076100 * t * t + 0.0020936 * t * t * t;
  }
  if (year < 1961.0) {
    t = year - 1950.0;
    return 29.07 + 0.407 * t - t * t / 233.0 + t * t * t / 2547.0;
  }
  if (year < 1986.0) {
    t = year - 1975.0;
    return 45.45 + 1.067 * t - t * t / 260.0 - t * t * t / 718.0;
  }
  if (year < 2005.0) {
    t = year - 2000.0;
    return 63.86 + 0.3345 * t - 0.060374 * t * t + 0.0017275 * t * t * t +
           0.000651814 * std::pow(t, 4) + 0.00002373599 * std::pow(t, 5);
  }
  if (year < 2050.0) {
    t = year - 2000.0;
    return 62.92 + 0.32217 * t + 0.005589 * t * t;
  }
  if (year <= 2150.0) {
    t = (year - 1820.0) / 100.0;
    return -20.0 + 32.0 * t * t - 0.5628 * (2150.0 - year);
  }
  t = (year - 1820.0) / 100.0;
  return -20.0 + 32.0 * t * t;
}

double TdbMinusTtSeconds(double tt_jd, double ut1_jd) {
  const double fractional_ut1 = ut1_jd - std::floor(ut1_jd - 0.5) - 0.5;
  return eraDtdb(2451545.0, tt_jd - 2451545.0, fractional_ut1, 0.0, 0.0, 0.0);
}

}  // namespace eclipse
