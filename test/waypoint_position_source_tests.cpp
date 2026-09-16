#include "WaypointPositionSource.h"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

namespace {
WaypointPosition Waypoint(const wxString& guid, const wxString& name,
                          double latitude, double longitude) {
  WaypointPosition waypoint;
  waypoint.guid = guid;
  waypoint.name = name;
  waypoint.latitude = latitude;
  waypoint.longitude = longitude;
  return waypoint;
}
}  // namespace

TEST(WaypointPositionSource, RejectsMissingIdentityAndInvalidCoordinates) {
  EXPECT_FALSE(WaypointPositionSource::IsUsable(Waypoint("", "No GUID", 1, 2)));
  EXPECT_FALSE(WaypointPositionSource::IsUsable(Waypoint(
      "nan", "Not finite", std::numeric_limits<double>::quiet_NaN(), 2)));
  EXPECT_FALSE(WaypointPositionSource::IsUsable(
      Waypoint("north", "Too far north", 90.0001, 2)));
  EXPECT_FALSE(WaypointPositionSource::IsUsable(
      Waypoint("east", "Too far east", 1, 180.0001)));
  EXPECT_TRUE(WaypointPositionSource::IsUsable(
      Waypoint("limit", "Valid limits", -90, 180)));
}

TEST(WaypointPositionSource, SortsNamesAndDeduplicatesGuids) {
  std::vector<WaypointPosition> input;
  input.push_back(Waypoint("3", "zulu", 3, 3));
  input.push_back(Waypoint("2", "Alpha", 2, 2));
  input.push_back(Waypoint("1", "alpha", 1, 1));
  input.push_back(Waypoint("2", "Duplicate", 20, 20));
  input.push_back(Waypoint("4", "", 4, 4));
  input.push_back(Waypoint("bad", "Invalid", 100, 0));

  const std::vector<WaypointPosition> result =
      WaypointPositionSource::Normalize(input);

  ASSERT_EQ(4u, result.size());
  EXPECT_EQ("Alpha", result[0].name);
  EXPECT_EQ("alpha", result[1].name);
  EXPECT_EQ("zulu", result[2].name);
  EXPECT_TRUE(result[3].name.empty());
  EXPECT_EQ(2.0, result[0].latitude);
}

TEST(WaypointPositionSource, ResolvesPersistedGuidWithoutRelyingOnName) {
  std::vector<WaypointPosition> waypoints;
  waypoints.push_back(Waypoint("holyhead", "Harbour", 53.31, -4.63));
  waypoints.push_back(Waypoint("foyle", "Harbour", 55.17, -7.04));

  WaypointPosition result;
  ASSERT_TRUE(WaypointPositionSource::FindByGuid(waypoints, "foyle", &result));
  EXPECT_EQ("Harbour", result.name);
  EXPECT_DOUBLE_EQ(55.17, result.latitude);
  EXPECT_DOUBLE_EQ(-7.04, result.longitude);
  EXPECT_FALSE(
      WaypointPositionSource::FindByGuid(waypoints, "deleted", &result));
  EXPECT_FALSE(WaypointPositionSource::FindByGuid(waypoints, "foyle", NULL));
}
