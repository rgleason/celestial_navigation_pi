#ifndef CELESTIAL_LUNAR_CANDIDATE_SELECTION_H
#define CELESTIAL_LUNAR_CANDIDATE_SELECTION_H

#include <cmath>
#include <limits>
#include "LunarDistanceEngine.h"

namespace lunar_distance {

inline bool ValidCandidatePosition(const GeographicPoint& p) {
  return std::isfinite(p.latitude_deg) && std::isfinite(p.longitude_deg) &&
         std::abs(p.latitude_deg) <= 90 && std::abs(p.longitude_deg) <= 180;
}

// Rank whole UTC/position branches, not just positions within the first root.
// A DR is a branch-selection aid, never a constraint on the measured solution.
// Preserve all roots and let an explicit user selection override the default.
inline int SelectLunarCandidate(const std::vector<TimeCandidate>& candidates,
                                const GeographicPoint* approximate = nullptr,
                                int preferred = -1) {
  if (preferred >= 0 && static_cast<std::size_t>(preferred) < candidates.size())
    return preferred;
  int selected = -1;
  double nearest_time = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    const double offset = std::abs(candidates[i].offset_seconds);
    if (std::isfinite(offset) && offset < nearest_time) {
      nearest_time = offset;
      selected = static_cast<int>(i);
    }
  }
  if (!approximate || !ValidCandidatePosition(*approximate)) return selected;
  double nearest = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    if (!std::isfinite(candidates[i].offset_seconds)) continue;
    for (const auto& position : candidates[i].positions) {
      if (!ValidCandidatePosition(position)) continue;
      const double distance = GreatCircleDistanceNm(*approximate, position);
      if (distance < nearest ||
          (distance == nearest && selected >= 0 &&
           std::abs(candidates[i].offset_seconds) <
               std::abs(candidates[selected].offset_seconds))) {
        nearest = distance;
        selected = static_cast<int>(i);
      }
    }
  }
  return selected;
}

}  // namespace lunar_distance
#endif
