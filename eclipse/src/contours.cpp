#include "eclipse/engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace eclipse {
namespace {

GeoPoint Interpolate(const GeoPoint& first, const GeoPoint& second,
                     double first_value, double second_value, double level) {
  double fraction = 0.5;
  if (second_value != first_value)
    fraction = (level - first_value) / (second_value - first_value);
  fraction = std::max(0.0, std::min(1.0, fraction));
  return GeoPoint(first.latitude_deg +
                      (second.latitude_deg - first.latitude_deg) * fraction,
                  first.longitude_deg +
                      (second.longitude_deg - first.longitude_deg) * fraction);
}

void AddSegment(int first_edge, int second_edge, const GeoPoint corners[4],
                const double values[4], double level,
                MagnitudeContour* contour) {
  static const int edge_corners[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
  const int a0 = edge_corners[first_edge][0];
  const int a1 = edge_corners[first_edge][1];
  const int b0 = edge_corners[second_edge][0];
  const int b1 = edge_corners[second_edge][1];
  ContourSegment segment;
  segment.first =
      Interpolate(corners[a0], corners[a1], values[a0], values[a1], level);
  segment.second =
      Interpolate(corners[b0], corners[b1], values[b0], values[b1], level);
  contour->segments.push_back(segment);
}

}  // namespace

bool EclipseEngine::BuildMagnitudeContours(
    const EclipseEvent& event, const std::vector<double>& levels,
    double grid_spacing_deg, double time_step_seconds,
    std::vector<MagnitudeContour>* contours, std::string* error) const {
  if (!contours || levels.empty() || grid_spacing_deg < 0.25 ||
      grid_spacing_deg > 10.0 || time_step_seconds < 10.0 ||
      time_step_seconds > 1800.0) {
    if (error) *error = "Invalid eclipse contour parameters";
    return false;
  }
  for (std::size_t index = 0; index < levels.size(); ++index) {
    if (levels[index] <= 0.0 || levels[index] >= 1.0) {
      if (error) *error = "Contour magnitudes must be between zero and one";
      return false;
    }
  }
  const int latitude_cells =
      static_cast<int>(std::ceil(180.0 / grid_spacing_deg));
  const int longitude_cells =
      static_cast<int>(std::ceil(360.0 / grid_spacing_deg));
  const double latitude_step = 180.0 / latitude_cells;
  const double longitude_step = 360.0 / longitude_cells;
  const int rows = latitude_cells + 1;
  const int columns = longitude_cells;
  std::vector<double> maximum(static_cast<std::size_t>(rows * columns), 0.0);

  const double time_step = time_step_seconds / 86400.0;
  const double start = event.maximum_tt_jd - 5.0 / 24.0;
  const double end = event.maximum_tt_jd + 5.0 / 24.0;
  for (double time = start; time <= end + time_step * 0.25; time += time_step) {
    SolarLunarState state;
    EarthOrientation orientation;
    if (!State(time, event.delta_t_seconds, &state, &orientation, error))
      return false;
    const EarthFixedSolarLunarState fixed = ToEarthFixed(state, orientation);
    for (int row = 0; row < rows; ++row) {
      const double latitude = -90.0 + row * latitude_step;
      for (int column = 0; column < columns; ++column) {
        const double longitude = -180.0 + column * longitude_step;
        const LocalCircumstances local = EvaluateLocalCircumstancesFixed(
            fixed, GeoPoint(latitude, longitude), 0.0, ellipsoid_, constants_);
        // Exclude a geometrical overlap hidden wholly below the horizon.
        if (local.sun_altitude_deg +
                RadiansToDegrees(local.sun_semidiameter_rad) <
            0.0)
          continue;
        double& value =
            maximum[static_cast<std::size_t>(row * columns + column)];
        value = std::max(value, local.magnitude);
      }
    }
  }

  contours->clear();
  contours->resize(levels.size());
  for (std::size_t level_index = 0; level_index < levels.size();
       ++level_index) {
    MagnitudeContour& contour = (*contours)[level_index];
    contour.magnitude = levels[level_index];
    for (int row = 0; row < latitude_cells; ++row) {
      const double south = -90.0 + row * latitude_step;
      const double north = south + latitude_step;
      for (int column = 0; column < columns; ++column) {
        const int next = (column + 1) % columns;
        const double west = -180.0 + column * longitude_step;
        const double east = west + longitude_step;
        const GeoPoint corners[4] = {
            GeoPoint(south, west), GeoPoint(south, east), GeoPoint(north, east),
            GeoPoint(north, west)};
        const double values[4] = {
            maximum[static_cast<std::size_t>(row * columns + column)],
            maximum[static_cast<std::size_t>(row * columns + next)],
            maximum[static_cast<std::size_t>((row + 1) * columns + next)],
            maximum[static_cast<std::size_t>((row + 1) * columns + column)]};
        int code = 0;
        for (int corner = 0; corner < 4; ++corner)
          if (values[corner] >= contour.magnitude) code |= 1 << corner;
        switch (code) {
          case 1:
            AddSegment(3, 0, corners, values, contour.magnitude, &contour);
            break;
          case 2:
            AddSegment(0, 1, corners, values, contour.magnitude, &contour);
            break;
          case 3:
            AddSegment(3, 1, corners, values, contour.magnitude, &contour);
            break;
          case 4:
            AddSegment(1, 2, corners, values, contour.magnitude, &contour);
            break;
          case 5:
            AddSegment(3, 2, corners, values, contour.magnitude, &contour);
            AddSegment(0, 1, corners, values, contour.magnitude, &contour);
            break;
          case 6:
            AddSegment(0, 2, corners, values, contour.magnitude, &contour);
            break;
          case 7:
            AddSegment(3, 2, corners, values, contour.magnitude, &contour);
            break;
          case 8:
            AddSegment(2, 3, corners, values, contour.magnitude, &contour);
            break;
          case 9:
            AddSegment(2, 0, corners, values, contour.magnitude, &contour);
            break;
          case 10:
            AddSegment(0, 3, corners, values, contour.magnitude, &contour);
            AddSegment(1, 2, corners, values, contour.magnitude, &contour);
            break;
          case 11:
            AddSegment(2, 1, corners, values, contour.magnitude, &contour);
            break;
          case 12:
            AddSegment(1, 3, corners, values, contour.magnitude, &contour);
            break;
          case 13:
            AddSegment(1, 0, corners, values, contour.magnitude, &contour);
            break;
          case 14:
            AddSegment(0, 3, corners, values, contour.magnitude, &contour);
            break;
          default:
            break;
        }
      }
    }
  }
  return true;
}

}  // namespace eclipse
