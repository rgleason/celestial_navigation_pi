#include "eclipse/dut1.h"
#include "eclipse/time.h"
#include <cmath>
#include <iterator>
#include <mutex>
#include <sstream>
#include <locale>
#include <stdexcept>

namespace eclipse {
namespace {
struct Record { int mjd; double dut1; char quality; };
const Record records[] = {
#include "dut1_data.inc"
};
std::mutex update_mutex;
std::shared_ptr<const Dut1Table> update_table;
double TaiAtMidnight(int mjd) {
  return TaiMinusUtcSeconds(JulianDateToCalendar(mjd+2400000.5));
}
}
double Dut1FirstUtcJd() { return records[0].mjd+2400000.5; }
double Dut1LastUtcJd() { return std::end(records)[-1].mjd+2400000.5; }
static Dut1Result LookupBundledDut1(double utc_jd) {
  Dut1Result result;
  if (!std::isfinite(utc_jd) || utc_jd<Dut1FirstUtcJd() || utc_jd>Dut1LastUtcJd())
    return result;
  const double mjd=utc_jd-2400000.5;
  const int day=static_cast<int>(std::floor(mjd));
  const auto& first=records[day-records[0].mjd];
  const double fraction=mjd-day;
  result.seconds=first.dut1;
  result.quality=first.quality;
  result.tai_minus_utc=TaiAtMidnight(day);
  if (fraction>0) {
    const auto& next=records[day-records[0].mjd+1];
    const double tai_first=TaiAtMidnight(day), tai_next=TaiAtMidnight(day+1);
    if (!std::isfinite(tai_first) || !std::isfinite(tai_next)) return Dut1Result();
    result.seconds += fraction*(next.dut1-first.dut1-(tai_next-tai_first));
    if (next.quality=='P') result.quality='P';
    else if (result.quality=='C' && next.quality=='I') result.quality='I';
  }
  result.available=true;
  return result;
}

std::shared_ptr<const Dut1Table> GetDut1Update() {
  std::lock_guard<std::mutex> lock(update_mutex);
  return update_table;
}
void SetDut1Update(std::shared_ptr<const Dut1Table> table) {
  std::lock_guard<std::mutex> lock(update_mutex);
  update_table=std::move(table);
}
Dut1Result LookupDut1(double utc_jd) {
  auto bundled=LookupBundledDut1(utc_jd);
  // Preserve the final C04 historical solution. Updates replace rapid and
  // predicted values, and extend coverage; they do not erase bundled history.
  if (bundled.available && bundled.quality=='C') return bundled;
  const auto update=GetDut1Update();
  if (update) {
    const auto value=update->Lookup(utc_jd);
    if (value.available) return value;
    if (utc_jd>update->LastUtcJd())
      bundled.tai_minus_utc=update->Lookup(update->LastUtcJd()).tai_minus_utc;
  }
  return bundled;
}
double Dut1Table::LastUtcJd() const {
  return days_.empty() ? 0 : days_.back().mjd+2400000.5;
}
Dut1Result Dut1Table::Lookup(double utc_jd) const {
  Dut1Result result;
  if (days_.empty() || !std::isfinite(utc_jd) ||
      utc_jd<days_.front().mjd+2400000.5 || utc_jd>LastUtcJd()) return result;
  const double mjd=utc_jd-2400000.5;
  const int day=static_cast<int>(std::floor(mjd));
  const auto& first=days_[day-days_.front().mjd];
  const double fraction=mjd-day;
  result.seconds=first.seconds;
  result.quality=first.quality;
  result.tai_minus_utc=first.tai_minus_utc;
  if (fraction>0) {
    const auto& next=days_[day-days_.front().mjd+1];
    result.seconds+=fraction*(next.seconds-first.seconds-(next.tai_minus_utc-first.tai_minus_utc));
    if (next.quality=='P') result.quality='P';
    else if (result.quality=='B' && next.quality=='I') result.quality='I';
  }
  result.available=true;
  result.from_update=true;
  return result;
}
bool Dut1Table::Equivalent(const Dut1Table& other) const {
  if (days_.size()!=other.days_.size()) return false;
  for (std::size_t i=0;i<days_.size();++i)
    if (days_[i].mjd!=other.days_[i].mjd || days_[i].seconds!=other.days_[i].seconds ||
        days_[i].quality!=other.days_[i].quality || days_[i].tai_minus_utc!=other.days_[i].tai_minus_utc) return false;
  return true;
}
std::shared_ptr<const Dut1Table> ParseDut1Update(const std::string& contents,std::string* error) {
  try {
    if (contents.size()>8*1024*1024 || contents.size()<1000000)
      throw std::runtime_error("Not a complete IERS finals2000A file (size)");
    auto number=[](const std::string& text) {
      std::istringstream input(text);
      input.imbue(std::locale::classic());
      double value;
      if (!(input>>value) || !std::isfinite(value) || !(input>>std::ws).eof())
        throw std::runtime_error("Invalid IERS numeric field");
      return value;
    };
    std::istringstream input(contents);
    std::string line;
    std::vector<Dut1Day> days;
    int tai=12; // 1973-01-02 anchor; subsequent leap jumps come from the data.
    while (std::getline(input,line)) {
      if (!line.empty() && line.back()=='\r') line.pop_back();
      if (line.empty()) continue;
      if (line.size()<68 || line.size()>250) throw std::runtime_error("Invalid IERS row width");
      if (line.substr(58,10).find_first_not_of(' ')==std::string::npos) continue;
      const double mjd=number(line.substr(7,8));
      if (mjd<41684 || mjd>200000 || mjd!=std::floor(mjd))
        throw std::runtime_error("Invalid IERS epoch");
      const int day=static_cast<int>(mjd);
      const auto date=JulianDateToCalendar(day+2400000.5);
      if (number(line.substr(0,2))!=date.year%100 ||
          number(line.substr(2,2))!=date.month || number(line.substr(4,2))!=date.day)
        throw std::runtime_error("IERS date/MJD mismatch");
      char quality=line[57];
      if (quality!='I' && quality!='P') throw std::runtime_error("Invalid IERS quality flag");
      double value=number(line.substr(58,10));
      if (line.size()>=165 && line.substr(154,11).find_first_not_of(' ')!=std::string::npos) {
        value=number(line.substr(154,11)); quality='B';
      }
      if (std::abs(value)>0.95 || (days.empty()?day!=41684:day!=days.back().mjd+1))
        throw std::runtime_error("Invalid or discontinuous DUT1 data");
      if (!days.empty()) {
        const double delta=value-days.back().seconds;
        const int jump=static_cast<int>(std::round(delta));
        if (std::abs(delta-jump)>0.05 || std::abs(jump)>1)
          throw std::runtime_error("Implausible daily DUT1 change");
        if (jump && !(date.day==1 && (date.month==1 || date.month==7)))
          throw std::runtime_error("Unexpected UTC leap date");
        tai+=jump;
      }
      if (date.year<=2025 && tai!=TaiMinusUtcSeconds(date))
        throw std::runtime_error("IERS UTC leap history mismatch");
      days.push_back({day,value,quality,tai});
    }
    if (days.size()<18000) throw std::runtime_error("Incomplete IERS daily coverage");
    if (error) error->clear();
    return std::shared_ptr<const Dut1Table>(new Dut1Table(std::move(days)));
  } catch (const std::exception& e) {
    if (error) *error=e.what();
    return nullptr;
  }
}
}
