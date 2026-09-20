#ifndef CELESTIAL_HORIZON_POSITION_H
#define CELESTIAL_HORIZON_POSITION_H

#include <algorithm>
#include <cmath>
#include <vector>

namespace horizon_position {

struct Position {
  double latitude;
  double longitude;
};

// Invert the astronomical triangle without selecting a latitude branch:
// sin(dec) = sin(lat) sin(h) + cos(lat) cos(h) cos(az).
// Bearings are clockwise from true north. Inputs and results are degrees.
// A degenerate continuum (equinox, h=0, az=90/270) has no isolated solution.
inline std::vector<Position> Candidates(double declination, double bodyLongitude,
                                        double altitude, double azimuth) {
  constexpr double radians = 3.14159265358979323846 / 180.0;
  std::vector<Position> result;
  if (!std::isfinite(declination) || !std::isfinite(bodyLongitude) ||
      !std::isfinite(altitude) || !std::isfinite(azimuth) ||
      std::abs(declination) >= 90.0 || std::abs(altitude) >= 90.0)
    return result;
  const double h = altitude * radians, a = azimuth * radians;
  const double x = std::cos(h) * std::cos(a), y = std::sin(h);
  const double radius = std::hypot(x, y);
  if (radius < 1e-12) return result;
  const double ratio = std::sin(declination * radians) / radius;
  if (std::abs(ratio) > 1.0 + 1e-12) return result;
  const double offset = std::atan2(y, x);
  const double boundedRatio = std::abs(1.0 - std::abs(ratio)) < 1e-14
                                  ? std::copysign(1.0, ratio)
                                  : std::max(-1.0, std::min(1.0, ratio));
  const double angle = std::acos(boundedRatio);
  for (double sign : {-1.0, 1.0}) {
    const double latitude = std::remainder((offset + sign * angle) / radians,
                                           360.0);
    // At a geographic pole true north, hence the observed azimuth, is undefined.
    if (std::abs(latitude) >= 90.0 - 1e-9) continue;
    const double phi = latitude * radians;
    const double hourAngle = std::atan2(
        -std::cos(h) * std::sin(a),
        std::sin(h) * std::cos(phi) -
            std::cos(h) * std::cos(a) * std::sin(phi));
    const double longitude =
        std::remainder(bodyLongitude + hourAngle / radians, 360.0);
    if (result.empty() || std::abs(latitude - result.front().latitude) > 1e-7)
      result.push_back({latitude, longitude});
  }
  std::sort(result.begin(), result.end(),
            [](const Position& lhs, const Position& rhs) {
              return lhs.latitude > rhs.latitude;
            });
  return result;
}

}  // namespace horizon_position

#endif
