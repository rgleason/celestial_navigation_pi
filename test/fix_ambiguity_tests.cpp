#include <gtest/gtest.h>
#include "FixAmbiguity.h"
#include <random>
using namespace fix_selection;

TEST(FixSelection, BothIntersectionsSatisfyIndependentSphericalDistances) {
  std::mt19937 generator(361);
  std::uniform_real_distribution<double> latitude(-85, 85),
      longitude(-180, 180);
  for (int trial = 0; trial < 500; ++trial) {
    Position truth{latitude(generator), longitude(generator)};
    Circle a{{latitude(generator), longitude(generator)}, 0};
    Circle b{{latitude(generator), longitude(generator)}, 0};
    a.altitude = 90 - DistanceNm(truth, a.body) / 60;
    b.altitude = 90 - DistanceNm(truth, b.body) / 60;
    const auto results = Intersect(a, b), reversed = Intersect(b, a);
    ASSERT_EQ(2u, results.positions.size()) << trial << ": " << results.error;
    ASSERT_EQ(2u, reversed.positions.size());
    double nearest = 1e20;
    for (const auto& p : results.positions) {
      EXPECT_TRUE(Valid(p));
      EXPECT_NEAR(90 - DistanceNm(p, a.body) / 60, a.altitude, 1e-7);
      EXPECT_NEAR(90 - DistanceNm(p, b.body) / 60, b.altitude, 1e-7);
      nearest = std::min(nearest, DistanceNm(truth, p));
      EXPECT_LT(std::min(DistanceNm(p, reversed.positions[0]),
                         DistanceNm(p, reversed.positions[1])),
                1e-5);
    }
    EXPECT_LT(nearest, 1e-5);
  }
}
TEST(FixSelection, SingularImpossibleAndTangentGeometryAreDistinguished) {
  EXPECT_TRUE(Intersect({{0, 0}, 30}, {{0, 0}, 30}).positions.empty());
  EXPECT_TRUE(Intersect({{0, 0}, 30}, {{0, 180}, 30}).positions.empty());
  EXPECT_TRUE(Intersect({{0, 0}, 60}, {{0, 90}, 60}).positions.empty());
  const auto tangent = Intersect({{0, 0}, 45}, {{0, 90}, 45});
  ASSERT_EQ(1u, tangent.positions.size());
  EXPECT_NEAR(0, tangent.positions[0].latitude, 1e-5);
  EXPECT_NEAR(45, tangent.positions[0].longitude, 1e-5);
  EXPECT_TRUE(Intersect({{NAN, 0}, 0}, {{0, 90}, 0}).positions.empty());
  EXPECT_TRUE(Intersect({{0, 0}, 91}, {{0, 90}, 0}).positions.empty());
}
TEST(FixSelection, DatelineAndAntipodalDistancesAreStable) {
  EXPECT_NEAR(12, DistanceNm({0, 179.9}, {0, -179.9}), 1e-9);
  EXPECT_NEAR(10800, DistanceNm({0, 0}, {0, 180}), 1e-9);
  EXPECT_NEAR(0, DistanceNm({90, 0}, {90, 180}), 1e-9);
}
TEST(FixSelection, LatestIncludedValidDrIsIndependentOfRowOrder) {
  std::vector<DrRecord> records = {
      {{7 + 20. / 60, 100 + 40. / 60}, 100, true, "Capella"},
      {{3, 99}, 200, true, "Schedar"},
      {{64, -41}, 300, false, "Hidden"},
      {{NAN, 0}, 400, true, "Invalid"}};
  EXPECT_EQ("Schedar", records[LatestDr(records)].key);
  std::reverse(records.begin(), records.end());
  EXPECT_EQ("Schedar", records[LatestDr(records)].key);
  for (auto& record : records) record.included = false;
  EXPECT_EQ(-1, LatestDr(records));
}
TEST(FixSelection,
     AcceptanceRequiresExplicitSelectionAndInvalidatesChangedInputs) {
  Acceptance state;
  state.SetInputs("DR+sights+epoch+motion");
  EXPECT_EQ(-1, state.Selected());
  state.Select(1);
  state.SetInputs("DR+sights+epoch+motion");
  EXPECT_EQ(1, state.Selected());
  state.SetInputs("different DR+sights+epoch+motion");
  EXPECT_EQ(-1, state.Selected());
  state.Select(0);
  state.Clear();
  EXPECT_EQ(-1, state.Selected());
}
