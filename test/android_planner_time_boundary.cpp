#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <iostream>

int main() {
  for (const char* zone : {"UTC", "Europe/London", "America/New_York", "Pacific/Auckland"}) {
    setenv("TZ", zone, 1); tzset();
    wxDateTime utc;
    assert(ParseNauticalPlannerInstant("2026-03-29", "01:30:00.125", PlannerTimeBasis::Utc, 0, &utc));
    assert(UtcDateTime::FormatInstant(utc, "%Y-%m-%d %H:%M:%S.%l") == "2026-03-29 01:30:00.125");
    const auto next = utc + wxTimeSpan::Hours(1);
    assert((next-utc).GetMilliseconds().GetValue() == 3600000);
    assert(ParseNauticalPlannerInstant("2026-03-29", "07:00:00.125", PlannerTimeBasis::ZoneTime, 5.5, &utc));
    assert(UtcDateTime::FormatInstant(utc, "%H:%M:%S.%l") == "01:30:00.125");
    assert(!ParseNauticalPlannerInstant("2026-02-30", "12:00:00", PlannerTimeBasis::Utc, 0, &utc));
    assert(!ParseNauticalPlannerInstant("2026-03-29", "01:30:00", PlannerTimeBasis::ZoneTime, 15, &utc));
    std::cout << zone << ": actual Android planner parser preserves UTC/zone fractions and absolute hour steps\n";
  }
  setenv("TZ", "Europe/London", 1); tzset();
  wxDateTime utc;
  assert(!ParseNauticalPlannerInstant("2026-03-29", "01:30:00", PlannerTimeBasis::ComputerLocal, 0, &utc));
  assert(!ParseNauticalPlannerInstant("2026-10-25", "01:30:00", PlannerTimeBasis::ComputerLocal, 0, &utc));
  assert(ParseNauticalPlannerInstant("2026-03-29", "03:30:00.875", PlannerTimeBasis::ComputerLocal, 0, &utc));
  assert(UtcDateTime::FormatInstant(utc, "%H:%M:%S.%l") == "02:30:00.875");
  std::cout << "Local gaps/overlaps refused; valid local fractional instant preserved\n";
}
