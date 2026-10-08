// Standalone Android UTC adapter regression. Compile on a Qt/wx developer host
// with __OCPN__ANDROID__; it uses the same adapter as the installed plugin.
#include "../src/UtcDateTime.h"
#include "../src/TimeStatus.h"
#include <wx/init.h>
#include <cstdlib>
#include <iostream>
#include <cassert>

int main() {
  wxInitializer wx;
  assert(wx.IsOk());
  const char* zones[] = {"UTC", "Europe/London", "America/New_York", "Pacific/Auckland"};
  for (const char* zone : zones) {
    setenv("TZ", zone, 1); tzset();
    // Independent POSIX epochs, not a round trip through the adapter under test.
    struct Case { int month, day; long long epoch; };
    for (const auto c : {Case{3,29,1774747812345LL}, Case{10,25,1792891812345LL},
                         Case{9,27,1790472612345LL}}) {
      const auto utc = UtcDateTime::Create(2026,c.month,c.day,1,30,12,345);
      assert(utc.GetValue().GetValue() == c.epoch);
      assert(UtcDateTime::ToInstant(utc).GetValue().GetValue() == c.epoch);
      const auto fields = UtcDateTime::Fields(utc);
      assert(fields.hour == 1 && fields.min == 30 && fields.sec == 12 && fields.msec == 345);
      wxDateTime reopened;
      assert(UtcDateTime::ParseUtc(UtcDateTime::FormatUtc(utc,"%Y-%m-%d %H:%M:%S.%l"), &reopened));
      assert(reopened.GetValue().GetValue() == c.epoch);
      assert(UtcDateTime::AddSeconds(utc,.655).GetValue().GetValue() == c.epoch + 655);
    }
    // Independent POSIX epoch values include fractional instants before1970.
    struct Historic { int year, month, day, hour, minute, second, ms;
                      long long epoch; const char* text; };
    for (const auto c : {
        Historic{1900,1,1,0,0,0,987,-2208988799013LL,"1900-01-01 00:00:00.987"},
        Historic{1969,12,31,23,59,59,987,-13LL,"1969-12-31 23:59:59.987"},
        Historic{1969,12,31,23,59,59,0,-1000LL,"1969-12-31 23:59:59.000"},
        Historic{1970,1,1,0,0,0,987,987LL,"1970-01-01 00:00:00.987"}}) {
      const auto utc = UtcDateTime::Create(c.year,c.month,c.day,c.hour,c.minute,c.second,c.ms);
      assert(utc.GetValue().GetValue() == c.epoch);
      assert(UtcDateTime::FormatUtc(utc,"%Y-%m-%d %H:%M:%S.%l") == c.text);
      wxDateTime reopened;
      assert(UtcDateTime::ParseUtc(c.text,&reopened));
      assert(reopened.GetValue().GetValue() == c.epoch);
    }
    wxDateTime gnss; wxString source;
    assert(GnssTimeMonitor::ParseNmeaUtc("$GPZDA,201530.25,04,07,2002,00,00*67", &gnss, &source));
    assert(gnss.GetValue().GetValue() == 1025813730250LL);
    std::cout << zone << ": UTC epochs, fractional arithmetic and saved-time parsing passed\n";
  }
  setenv("TZ", "Europe/London", 1); tzset();
  assert(!UtcDateTime::LocalWallToInstant(UtcDateTime::Create(2026,3,29,1,30)).IsValid());
  assert(!UtcDateTime::LocalWallToInstant(UtcDateTime::Create(2026,10,25,1,30)).IsValid());
  // POBsoft (1985–2026): independent zoneinfo epochs on the valid side of
  // both transitions, with milliseconds; also exercise a half-hour change.
  assert(UtcDateTime::LocalWallToInstant(UtcDateTime::Create(2026,3,29,2,30,12,987))
             .GetValue().GetValue() == 1774747812987LL);
  assert(UtcDateTime::LocalWallToInstant(UtcDateTime::Create(2026,10,25,2,30,12,987))
             .GetValue().GetValue() == 1792895412987LL);
  setenv("TZ", "Australia/Lord_Howe", 1); tzset();
  assert(!UtcDateTime::LocalWallToInstant(UtcDateTime::Create(2026,10,4,2,15)).IsValid());
  assert(!UtcDateTime::LocalWallToInstant(UtcDateTime::Create(2026,4,5,1,45)).IsValid());
  assert(UtcDateTime::LocalWallToInstant(UtcDateTime::Create(2026,10,4,2,45,12,987))
             .GetValue().GetValue() == 1791042312987LL);
  assert(UtcDateTime::FormatUtc(UtcDateTime::Create(2026,9,27,9,3,12,345),"%Y-%m-%d %H:%M:%S.%l") == "2026-09-27 09:03:12.345");
  assert(!UtcDateTime::Create(2026,2,30).IsValid());
  std::cout << "DST gap and ambiguous overlap refused; UTC report label and invalid date passed\n";
}
