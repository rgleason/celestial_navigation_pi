#include "CompactEphemerisProvider.h"
#include <gtest/gtest.h>
#include "eclipse/dut1.h"
#include "eclipse/time.h"
#include "NavigationEphemerisProvider.h"
#include "Dut1UpdatePanel.h"
#include "AtomicXmlFile.h"
#include <wx/file.h>
#include <wx/filename.h>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <vector>

namespace {
// Synthetic full-format input derived from bundled daily history. Future rows
// exercise availability and a hypothetical 2030 leap, NOT future accuracy.
std::string Fixture(int last=64328) { // through 2035
  std::ostringstream out;
  out.imbue(std::locale::classic());
  double value=0;
  for (int day=41684;day<=last;++day) {
    const auto date=eclipse::JulianDateToCalendar(day+2400000.5);
    const auto bundled=eclipse::LookupDut1(day+2400000.5);
    if (bundled.available) value=bundled.seconds;
    else if (day<62502) value+=std::clamp(-0.6-value,-0.001,0.001);
    else if (day==62502) value+=1; // 2030-01-01
    out<<std::setfill('0')<<std::setw(2)<<date.year%100<<std::setfill(' ')
       <<std::setw(2)<<date.month<<std::setw(2)<<date.day<<' '
       <<std::fixed<<std::setprecision(2)<<std::setw(8)<<double(day)
       <<std::string(42,' ')<<'I'<<std::setprecision(7)<<std::setw(10)<<value
       <<std::string(119,' ')<<'\n';
  }
  return out.str();
}
}
TEST(Dut1Update, ParsesHistoryAndFutureLeapWithoutSmearing) {
  std::string error;
  const auto table=eclipse::ParseDut1Update(Fixture(),&error);
  ASSERT_TRUE(table) << error;
  const auto before=table->Lookup(62502+2400000.5-0.5);
  const auto after=table->Lookup(62502+2400000.5);
  EXPECT_NEAR(before.seconds,-0.6,1e-7);
  EXPECT_NEAR(after.seconds,0.4,1e-7);
  EXPECT_EQ(before.tai_minus_utc,37);
  EXPECT_EQ(after.tai_minus_utc,38);
  EXPECT_TRUE(after.from_update);
  EXPECT_FALSE(table->Lookup(table->LastUtcJd()+0.01).available);
  EXPECT_FALSE(table->Lookup(NAN).available);
  // Optional release integration check using an unchanged official download.
  if (const char* path=std::getenv("CELESTIAL_IERS_TEST_FILE")) {
    std::ifstream file(path,std::ios::binary);
    ASSERT_TRUE(file);
    const std::string official((std::istreambuf_iterator<char>(file)),{});
    const auto actual=eclipse::ParseDut1Update(official,&error);
    ASSERT_TRUE(actual) << error;
    EXPECT_GE(actual->LastUtcJd(),eclipse::Dut1LastUtcJd());
    EXPECT_EQ(actual->Lookup(2457754.5).tai_minus_utc,37);
    EXPECT_NEAR(actual->Lookup(2457754.5).seconds,0.59122,0.001);
  }
}
TEST(Dut1Update, AnalyticalEpochUsesUpdateAndRetainsLastKnownLeapAfterCoverage) {
  const auto previous = eclipse::GetDut1Update();
  struct Restore {
    std::shared_ptr<const eclipse::Dut1Table> previous;
    ~Restore() { eclipse::SetDut1Update(previous); }
  } restore{previous};
  std::string error;
  const auto table = eclipse::ParseDut1Update(Fixture(), &error);
  ASSERT_TRUE(table) << error;
  eclipse::SetDut1Update(table);
  celestial_navigation::AnalyticalNavigationEpoch epoch;
  ASSERT_TRUE(celestial_navigation::ResolveAnalyticalNavigationEpoch(
      wxDateTime(1, wxDateTime::Jan, 2030, 0, 0, 0), &epoch));
  EXPECT_TRUE(epoch.dut1_available);
  EXPECT_TRUE(epoch.dut1_from_update);
  EXPECT_NEAR(epoch.dut1_seconds, 0.4, 1e-6);
  EXPECT_NEAR((epoch.tt_jd - epoch.utc_jd) * 86400.0, 70.184, 0.00005);
  ASSERT_TRUE(celestial_navigation::ResolveAnalyticalNavigationEpoch(
      wxDateTime(1, wxDateTime::Jan, 2040, 0, 0, 0), &epoch));
  EXPECT_FALSE(epoch.dut1_available);
  EXPECT_DOUBLE_EQ(epoch.dut1_seconds, 0.0);
  EXPECT_NEAR((epoch.tt_jd - epoch.utc_jd) * 86400.0, 70.184, 0.00005);
}
TEST(Dut1Update, RejectsTruncationGarbageAndDamagedEpochs) {
  const auto valid=Fixture();
  std::string error;
  for (auto bad : {std::string("<html>no data</html>"),valid.substr(0,100000),valid}) {
    if (bad.size()==valid.size()) bad.replace(7,8,"99999999");
    EXPECT_FALSE(eclipse::ParseDut1Update(bad,&error));
    EXPECT_FALSE(error.empty());
  }
  auto bad=valid;
  bad[57]='X';
  EXPECT_FALSE(eclipse::ParseDut1Update(bad,&error));
  bad=valid; bad.replace(58,10,"       nan");
  EXPECT_FALSE(eclipse::ParseDut1Update(bad,&error));
  bad=valid; bad.erase(188,188);
  EXPECT_FALSE(eclipse::ParseDut1Update(bad,&error));
}
TEST(Dut1Update, InstallIsAtomicAndPreservesBundleAndLastGoodUpdate) {
  const wxString input=wxFileName::CreateTempFileName("dut1-input-");
  const wxString destination=wxFileName::CreateTempFileName("dut1-cache-");
  const auto bytes=Fixture();
  wxString message;
  ASSERT_TRUE(celestial_navigation::ReplaceFileAtomically(input,[&](wxTempFile& file) {
    return file.Write(bytes.data(),bytes.size());
  }));
  const auto historical=eclipse::LookupDut1(2457754.5);
  ASSERT_TRUE(celestial_navigation::InstallDut1Update(input,destination,&message)) << message;
  const auto installed=eclipse::GetDut1Update();
  ASSERT_TRUE(installed);
  EXPECT_DOUBLE_EQ(eclipse::LookupDut1(2457754.5).seconds,historical.seconds);
  EXPECT_FALSE(eclipse::LookupDut1(2457754.5).from_update);
  EXPECT_TRUE(eclipse::LookupDut1(2464000.5).from_update);
  const auto future=eclipse::LookupDut1(installed->LastUtcJd()+365);
  EXPECT_FALSE(future.available);
  EXPECT_EQ(future.seconds,0);
  EXPECT_EQ(future.tai_minus_utc,38);
  EXPECT_TRUE(celestial_navigation::InstallDut1Update(input,destination,&message));
  EXPECT_TRUE(message.Contains("already up to date"));
  const auto newer=Fixture(64329);
  ASSERT_TRUE(celestial_navigation::ReplaceFileAtomically(input,[&](wxTempFile& file) {
    return file.Write(newer.data(),newer.size());
  }));
  // A regular file cannot be the parent of a cache file: persistence must fail
  // without activating the newer in-memory table.
  EXPECT_FALSE(celestial_navigation::InstallDut1Update(input,destination+"/blocked",&message));
  EXPECT_EQ(eclipse::GetDut1Update(),installed);
  const auto older=Fixture(61000);
  ASSERT_TRUE(celestial_navigation::ReplaceFileAtomically(input,[&](wxTempFile& file) {
    return file.Write(older.data(),older.size());
  }));
  EXPECT_FALSE(celestial_navigation::InstallDut1Update(input,destination,&message));
  EXPECT_EQ(eclipse::GetDut1Update(),installed);
  ASSERT_TRUE(celestial_navigation::ReplaceFileAtomically(input,[](wxTempFile& file) {
    return file.Write("damaged",7);
  }));
  EXPECT_FALSE(celestial_navigation::InstallDut1Update(input,destination,&message));
  EXPECT_EQ(eclipse::GetDut1Update(),installed);
  wxFile file(destination);
  EXPECT_EQ(file.Length(),static_cast<wxFileOffset>(bytes.size()));
  file.Close();
  eclipse::SetDut1Update(nullptr);
  wxRemoveFile(input); wxRemoveFile(destination);
}

TEST(Dut1Update, PublishesUpdatesSafelyAcrossThreads) {
  std::string error;
  const auto table=eclipse::ParseDut1Update(Fixture(),&error);
  ASSERT_TRUE(table) << error;
  eclipse::SetDut1Update(table);

  std::atomic<bool> start(false);
  std::atomic<int> failures(0);
  std::vector<std::thread> readers;
  for (int thread=0;thread<4;++thread) {
    readers.emplace_back([&]() {
      while (!start.load(std::memory_order_acquire)) {}
      for (int iteration=0;iteration<5000;++iteration) {
        const auto current=eclipse::GetDut1Update();
        if (current && current!=table) ++failures;
        const auto value=eclipse::LookupDut1(2464000.5);
        if (value.available && !value.from_update) ++failures;
      }
    });
  }
  std::thread writer([&]() {
    while (!start.load(std::memory_order_acquire)) {}
    for (int iteration=0;iteration<5000;++iteration)
      eclipse::SetDut1Update(iteration%2 ? table : nullptr);
  });
  start.store(true,std::memory_order_release);
  writer.join();
  for (auto& reader : readers) reader.join();
  EXPECT_EQ(failures.load(),0);
  eclipse::SetDut1Update(nullptr);
}

TEST(Dut1Update, CompactUsesImportedLeapHistoryAndFreezesLunarTimeData) {
  const auto previous=eclipse::GetDut1Update();
  struct Restore { std::shared_ptr<const eclipse::Dut1Table> value;
    ~Restore(){eclipse::SetDut1Update(value);} } restore{previous};
  std::string error;
  const auto update=eclipse::ParseDut1Update(Fixture(),&error);
  ASSERT_TRUE(update) << error;
  eclipse::SetDut1Update(update);
  const wxDateTime utc(1,wxDateTime::Jan,2030,12,0,0);
  const auto request=celestial_navigation::CompactRequest("Moon",utc,NAN,update);
  EXPECT_DOUBLE_EQ(request.tai_minus_utc,38.0);
  EXPECT_NEAR(request.dut1_seconds,.4,1e-6);
  const auto later=celestial_navigation::CompactRequest("Moon",
      wxDateTime(1,wxDateTime::Jan,2040,12,0,0),NAN,update);
  EXPECT_DOUBLE_EQ(later.tai_minus_utc,38.0);
  EXPECT_DOUBLE_EQ(later.dut1_seconds,0.0);
  const auto session=celestial_navigation::SelectEnhancedLunarProvider(
      "Sun",utc,-60,60,false,true,update);
  ASSERT_TRUE(session.used_compact); ASSERT_TRUE(session.ephemeris);
  lunar_distance::EphemerisSample a,b;
  ASSERT_TRUE(session.ephemeris(0,&a,&error));
  eclipse::SetDut1Update(nullptr);
  ASSERT_TRUE(session.ephemeris(0,&b,&error));
  EXPECT_DOUBLE_EQ(a.predicted_distance_deg,b.predicted_distance_deg);
  EXPECT_DOUBLE_EQ(a.moon_geographic_longitude_deg,b.moon_geographic_longitude_deg);
  EXPECT_DOUBLE_EQ(a.dut1_seconds,b.dut1_seconds);
  EXPECT_TRUE(b.dut1_from_update);
}
