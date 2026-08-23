#ifndef CELESTIAL_NAVIGATION_WAYPOINT_POSITION_SOURCE_H
#define CELESTIAL_NAVIGATION_WAYPOINT_POSITION_SOURCE_H

#include <wx/string.h>

#include <vector>

struct WaypointPosition {
  wxString guid;
  wxString name;
  double latitude;
  double longitude;
};

class WaypointPositionSource {
public:
  static bool IsUsable(const WaypointPosition& waypoint);
  static std::vector<WaypointPosition> Normalize(
      const std::vector<WaypointPosition>& waypoints);
  static bool FindByGuid(const std::vector<WaypointPosition>& waypoints,
                         const wxString& guid, WaypointPosition* result);
};

#endif
