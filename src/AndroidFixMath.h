// POBsoft (1985-2026). Android correction for the legacy Cone 2 derivative.
#pragma once
#ifdef __OCPN__ANDROID__
#include <array>
#include <cmath>

namespace celestial_android {
// Gradient of dot(body, position) / length(position). The legacy diagonal
// expression omitted cross terms and did not converge even from a nearby DR.
inline std::array<double, 3> ConeAltitudeGradient(
    const std::array<double, 3>& body,
    const std::array<double, 3>& position) {
  const double squared = position[0] * position[0] +
      position[1] * position[1] + position[2] * position[2];
  const double length = std::sqrt(squared);
  const double dot = body[0] * position[0] + body[1] * position[1] +
      body[2] * position[2];
  return {{body[0] / length - dot * position[0] / (length * squared),
           body[1] / length - dot * position[1] / (length * squared),
           body[2] / length - dot * position[2] / (length * squared)}};
}
}  // namespace celestial_android
#endif
