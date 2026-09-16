#ifndef CELESTIAL_ECLIPSE_DUT1_H
#define CELESTIAL_ECLIPSE_DUT1_H
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <limits>
namespace eclipse {
struct Dut1Result {
  bool available = false;
  double seconds = 0;
  char quality = '?';  // C=final C04, I=rapid, P=predicted
  bool from_update = false;
  // A downloaded table's last known UTC offset remains usable after DUT1
  // coverage ends. NaN means the caller must use its built-in leap history.
  double tai_minus_utc = std::numeric_limits<double>::quiet_NaN();
};
// Bundled daily IERS data; interpolate UT1-TAI, never across the UTC jump.
// Input follows CalendarToJulianDate's UTC convention (no 23:59:60 instant).
// Outside coverage, return unavailable and zero: no silent extrapolation.
Dut1Result LookupDut1(double utc_jd);
double Dut1FirstUtcJd();
double Dut1LastUtcJd();
struct Dut1Day { int mjd; double seconds; char quality; int tai_minus_utc; };
class Dut1Table {
 public:
  Dut1Result Lookup(double utc_jd) const;
  double LastUtcJd() const;
  bool Equivalent(const Dut1Table& other) const;
 private:
  friend std::shared_ptr<const Dut1Table> ParseDut1Update(const std::string&, std::string*);
  explicit Dut1Table(std::vector<Dut1Day> days) : days_(std::move(days)) {}
  std::vector<Dut1Day> days_;
};
// Strictly validate the complete official finals2000A format before activation.
std::shared_ptr<const Dut1Table> ParseDut1Update(const std::string& contents,
                                              std::string* error);
std::shared_ptr<const Dut1Table> GetDut1Update();
void SetDut1Update(std::shared_ptr<const Dut1Table> table);
}
#endif
