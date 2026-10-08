// Saved-sight DR selection for the FIX dialog.
#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace fix_dr {
struct Position {
  double latitude, longitude;
  Position(double lat = NAN, double lon = NAN)
      : latitude(lat), longitude(lon) {}
};
inline bool Valid(const Position& p) {
  return std::isfinite(p.latitude) && std::isfinite(p.longitude) &&
         std::abs(p.latitude) <= 90 && std::abs(p.longitude) <= 180;
}
struct DrRecord {
  Position position;
  std::int64_t utcMilliseconds;
  bool included;
  std::string key;
  DrRecord(Position p = {}, std::int64_t utc = 0, bool include = false,
           std::string id = {})
      : position(p), utcMilliseconds(utc), included(include), key(id) {}
};
// No valid saved DR means no default, never an implicit boat-position fallback.
inline int LatestDr(const std::vector<DrRecord>& records) {
  int selected = -1;
  for (std::size_t i = 0; i < records.size(); ++i) {
    const auto& r = records[i];
    if (!r.included || !Valid(r.position)) continue;
    if (selected < 0 || r.utcMilliseconds > records[selected].utcMilliseconds ||
        (r.utcMilliseconds == records[selected].utcMilliseconds &&
         r.key < records[selected].key))
      selected = static_cast<int>(i);
  }
  return selected;
}
}  // namespace fix_dr
