#include "WaypointPositionSource.h"

#include <algorithm>
#include <cmath>

bool WaypointPositionSource::IsUsable(const WaypointPosition& waypoint) {
  return !waypoint.guid.empty() && std::isfinite(waypoint.latitude) &&
         std::isfinite(waypoint.longitude) && waypoint.latitude >= -90.0 &&
         waypoint.latitude <= 90.0 && waypoint.longitude >= -180.0 &&
         waypoint.longitude <= 180.0;
}

std::vector<WaypointPosition> WaypointPositionSource::Normalize(
    const std::vector<WaypointPosition>& waypoints) {
  std::vector<WaypointPosition> result;
  for (std::vector<WaypointPosition>::const_iterator candidate =
           waypoints.begin();
       candidate != waypoints.end(); ++candidate) {
    if (!IsUsable(*candidate)) continue;
    bool duplicate = false;
    for (std::vector<WaypointPosition>::const_iterator existing =
             result.begin();
         existing != result.end(); ++existing) {
      if (existing->guid == candidate->guid) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) result.push_back(*candidate);
  }

  std::sort(result.begin(), result.end(),
            [](const WaypointPosition& left, const WaypointPosition& right) {
              if (left.name.empty() != right.name.empty())
                return !left.name.empty();
              const int insensitive = left.name.CmpNoCase(right.name);
              if (insensitive != 0) return insensitive < 0;
              const int sensitive = left.name.Cmp(right.name);
              if (sensitive != 0) return sensitive < 0;
              return left.guid.Cmp(right.guid) < 0;
            });
  return result;
}

bool WaypointPositionSource::FindByGuid(
    const std::vector<WaypointPosition>& waypoints, const wxString& guid,
    WaypointPosition* result) {
  if (guid.empty() || !result) return false;
  for (std::vector<WaypointPosition>::const_iterator waypoint =
           waypoints.begin();
       waypoint != waypoints.end(); ++waypoint) {
    if (waypoint->guid == guid && IsUsable(*waypoint)) {
      *result = *waypoint;
      return true;
    }
  }
  return false;
}
