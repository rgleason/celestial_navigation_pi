#include <gtest/gtest.h>
#include "AndroidAlmanacCsv.h"

TEST(AndroidAlmanacCsv, RetainsEachFractionalInstantAndExistingNumericColumns) {
  // Epoch independently checked as 2024-10-27 01:30:12 UTC, in the repeated
  // local hour of the London autumn transition. Do not construct local fields.
  const wxDateTime epoch(wxLongLong(1729992612000LL));
  AlmanacRow a{};
  a.utc = epoch + wxTimeSpan::Milliseconds(987);
  a.body = "Moon";
  a.declination = -23.123456;
  a.altitude = -0.012345;
  AlmanacRow b = a;
  b.utc = epoch + wxTimeSpan::Milliseconds(123);
  b.body = "Sun";
  AlmanacRow c = a;
  c.utc = epoch;
  c.body = "Polaris";
  const std::vector<AlmanacRow> rows{a, b, c};
  const wxString output = AndroidAlmanacCsv(rows);
  EXPECT_NE(wxNOT_FOUND, output.Find("2024-10-27T01:30:12.987Z,Moon,"));
  EXPECT_NE(wxNOT_FOUND, output.Find("2024-10-27T01:30:12.123Z,Sun,"));
  EXPECT_NE(wxNOT_FOUND, output.Find("2024-10-27T01:30:12Z,Polaris,"));
  wxString old = AlmanacToCsv(rows).AfterFirst('\n');
  wxString extended = output.AfterFirst('\n');
  for (size_t i = 0; i < rows.size(); ++i) {
    EXPECT_EQ(old.BeforeFirst('\n').AfterFirst(','),
              extended.BeforeFirst('\n').AfterFirst(','));
    old = old.AfterFirst('\n');
    extended = extended.AfterFirst('\n');
  }
  EXPECT_EQ(AlmanacToCsv({}), AndroidAlmanacCsv({}));
  EXPECT_EQ(AlmanacToCsv({c}), AndroidAlmanacCsv({c}));
}
