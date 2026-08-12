#ifndef CELESTIAL_ECLIPSE_TIME_H
#define CELESTIAL_ECLIPSE_TIME_H

#include <string>

namespace eclipse {

struct CalendarDateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  double second;

  CalendarDateTime()
      : year(2000), month(1), day(1), hour(0), minute(0), second(0.0) {}
};

bool CalendarToJulianDate(const CalendarDateTime& calendar, double* jd,
                          std::string* error);
CalendarDateTime JulianDateToCalendar(double jd);

// Morrison/Stephenson/Espenak-Meeus polynomial model. This is a prediction,
// not a statement that future Earth rotation or future UTC leap seconds are
// known. UI and exports must label it as such and permit an override.
double ModelDeltaTSeconds(double decimal_year);
double DecimalYear(const CalendarDateTime& calendar);

// Approximate TDB-TT for a geocentric observer using ERFA's Fairhead-Bretagnon
// model. Sub-millisecond observer-location terms are irrelevant to global
// eclipse paths.
double TdbMinusTtSeconds(double tt_jd, double ut1_jd);

}  // namespace eclipse

#endif

