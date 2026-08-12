#include "eclipse/time.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

struct TimeInvariantTests {
  TimeInvariantTests() {
    eclipse::CalendarDateTime calendar;
    calendar.year = 2027;
    calendar.month = 8;
    calendar.day = 2;
    calendar.hour = 10;
    std::string error;
    double jd = 0.0;
    if (!eclipse::CalendarToJulianDate(calendar, &jd, &error) ||
        std::fabs(jd - 2461619.9166666665) > 1e-9) {
      std::cerr << "FAIL calendar conversion: " << error << '\n';
      std::exit(EXIT_FAILURE);
    }
    const eclipse::CalendarDateTime round_trip =
        eclipse::JulianDateToCalendar(jd);
    if (round_trip.year != 2027 || round_trip.month != 8 ||
        round_trip.day != 2 || round_trip.hour != 10) {
      std::cerr << "FAIL calendar round trip: " << round_trip.year << '-'
                << round_trip.month << '-' << round_trip.day << ' '
                << round_trip.hour << ':' << round_trip.minute << ':'
                << round_trip.second << '\n';
      std::exit(EXIT_FAILURE);
    }
    const double delta_t = eclipse::ModelDeltaTSeconds(2027.5);
    if (delta_t < 65.0 || delta_t > 80.0) {
      std::cerr << "FAIL implausible 2027 Delta-T model: " << delta_t << '\n';
      std::exit(EXIT_FAILURE);
    }
  }
};

TimeInvariantTests time_invariant_tests;

}  // namespace
