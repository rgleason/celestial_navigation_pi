#include <gtest/gtest.h>
#include "eclipse/dut1.h"
#include "eclipse/time.h"
#include "Dut1UpdatePanel.h"
#include "AtomicXmlFile.h"
#include <wx/file.h>
#include <wx/filename.h>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>

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
