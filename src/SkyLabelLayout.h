#ifndef CELESTIAL_NAVIGATION_SKY_LABEL_LAYOUT_H
#define CELESTIAL_NAVIGATION_SKY_LABEL_LAYOUT_H

#include <wx/gdicmn.h>
#include <algorithm>
#include <vector>

namespace sky_labels {
// Density is independent of the magnitude filter. More means as many names
// as fit, never permission to overlap them or obscure a body marker.
inline unsigned Budget(const wxSize& size, int density) {
  const int divisors[] = {24000, 7000, 3000};
  const int minimums[] = {4, 8, 12};
  density = std::max(0, std::min(2, density));
  const long area = static_cast<long>(std::max(0, size.x)) * std::max(0, size.y);
  return static_cast<unsigned>(std::max<long>(minimums[density], area / divisors[density]));
}

inline bool Place(const wxPoint& point, const wxSize& extent,
                  const wxRect& bounds, const std::vector<wxRect>& occupied,
                  const std::vector<wxRect>& markers, wxRect* result,
                  bool searchWholePlot = false) {
  if (!result || extent.x > bounds.width || extent.y > bounds.height) return false;
  auto fits = [&](int x, int y) {
    wxRect label(x, y, extent.x, extent.y), padded = label;
    padded.Inflate(2);
    if (!bounds.Contains(padded)) return false;
    for (const auto& used : occupied) if (used.Intersects(padded)) return false;
    for (const auto& marker : markers) if (marker.Intersects(padded)) return false;
    *result = label;
    return true;
  };
  if (fits(point.x + 8, point.y - extent.y / 2) ||
      fits(point.x - extent.x - 8, point.y - extent.y / 2) ||
      fits(point.x - extent.x / 2, point.y - extent.y - 8) ||
      fits(point.x - extent.x / 2, point.y + 8)) return true;
  // A short leader connects offset names to their actual dots in crowded areas.
  for (int row = 1; row <= 5; ++row)
    for (int sign : {-1, 1})
      for (int side : {1, -1})
        if (fits(point.x + (side > 0 ? 8 : -extent.x - 8),
                 point.y + sign * row * (extent.y + 4) - extent.y / 2)) return true;
  if (searchWholePlot)
    for (int y = bounds.y + 2; y + extent.y + 2 <= bounds.y + bounds.height; y += 4)
      for (int x = bounds.x + 2; x + extent.x + 2 <= bounds.x + bounds.width; x += 4)
        if (fits(x, y)) return true;
  return false;
}
}  // namespace sky_labels
#endif
