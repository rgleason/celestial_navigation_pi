#include <gtest/gtest.h>
#include <algorithm>
#include "FixDrSource.h"

using namespace fix_dr;

TEST(FixDr, LatestIncludedSavedPositionIsIndependentOfRowOrder) {
  std::vector<DrRecord> records = {
      {{7 + 20. / 60, 100 + 40. / 60}, 100, true, "Capella"},
      {{3, 99}, 200, true, "Schedar"},
      {{64, -41}, 300, false, "Excluded"},
      {{NAN, 0}, 400, true, "Invalid"}};
  ASSERT_GE(LatestDr(records), 0);
  EXPECT_EQ("Schedar", records[LatestDr(records)].key);
  std::reverse(records.begin(), records.end());
  EXPECT_EQ("Schedar", records[LatestDr(records)].key);
  records[LatestDr(records)].included = false;
  EXPECT_EQ("Capella", records[LatestDr(records)].key);
}

TEST(FixDr, MissingOrInvalidSavedPositionsHaveNoImplicitDefault) {
  EXPECT_EQ(-1, LatestDr({}));
  EXPECT_EQ(-1, LatestDr({{{64, -41}, 1, false, "Excluded"},
                          {{NAN, 100}, 2, true, "Missing latitude"},
                          {{7, INFINITY}, 3, true, "Invalid longitude"},
                          {{91, 100}, 4, true, "Outside latitude range"},
                          {{7, 181}, 5, true, "Outside longitude range"}}));
  // A real position at the equator/prime meridian remains valid.
  EXPECT_EQ(0, LatestDr({{{0, 0}, 1, true, "Saved zero position"}}));
}

TEST(FixDr, EqualTimesUseStableSavedIdentity) {
  std::vector<DrRecord> records = {{{7, 100}, 1, true, "B"},
                                   {{8, 101}, 1, true, "A"}};
  EXPECT_EQ("A", records[LatestDr(records)].key);
  std::reverse(records.begin(), records.end());
  EXPECT_EQ("A", records[LatestDr(records)].key);
}
