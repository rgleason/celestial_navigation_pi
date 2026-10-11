#ifndef CELESTIAL_SIGHT_OVERLAY_H
#define CELESTIAL_SIGHT_OVERLAY_H
#include <algorithm>
#include <cmath>
#include <wx/gdicmn.h>

struct SightDisplayStyle {
  double lineWidthMm = 0.5;
  int bandOpacityPercent = 100;
  bool contrastHalo = true;
  bool hoverLabels = true;
};

inline double SightSegmentDistance(const wxPoint& point, const wxPoint& a,
                                   const wxPoint& b) {
  const double dx = double(b.x) - a.x, dy = double(b.y) - a.y;
  const double length2 = dx * dx + dy * dy;
  const double t = length2 > 0 ? std::max(0.0, std::min(1.0,
      ((double(point.x) - a.x) * dx + (double(point.y) - a.y) * dy) / length2)) : 0;
  return std::hypot(point.x - (a.x + t * dx), point.y - (a.y + t * dy));
}
#endif
