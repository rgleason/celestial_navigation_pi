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
    wxDateTime gnss; wxString source;
    assert(GnssTimeMonitor::ParseNmeaUtc("$GPZDA,201530.25,04,07,2002,00,00*67", &gnss, &source));
    assert(gnss.GetValue().GetValue() == 1025813730250LL);
    std::cout << zone << ": UTC epochs, fractional arithmetic and saved-time parsing passed\n";
  }
  setenv("TZ", "Europe/London", 1); tzset();
  assert(!UtcDateTime::LocalWallToInstant(UtcDateTime::Create(2026,3,29,1,30)).IsValid());
  assert(!UtcDateTime::LocalWallToInstant(UtcDateTime::Create(2026,10,25,1,30)).IsValid());
  assert(UtcDateTime::FormatUtc(UtcDateTime::Create(2026,9,27,9,3,12,345),"%Y-%m-%d %H:%M:%S.%l") == "2026-09-27 09:03:12.345");
  assert(!UtcDateTime::Create(2026,2,30).IsValid());
  std::cout << "DST gap and ambiguous overlap refused; UTC report label and invalid date passed\n";
}
