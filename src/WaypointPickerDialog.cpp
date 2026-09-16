#include "WaypointPickerDialog.h"
#include "OcpnApiCompat.h"
std::vector<WaypointPosition> LoadOpenCpnWaypoints() {
  std::vector<WaypointPosition> positions;
  const auto guids = GetWaypointGUIDArray();
  for (const auto& guid : guids) {
    PlugIn_Waypoint waypoint;
    if (GetSingleWaypoint(guid, &waypoint))
      positions.push_back(
          {guid, waypoint.m_MarkName, waypoint.m_lat, waypoint.m_lon});
  }
  return WaypointPositionSource::Normalize(positions);
}
