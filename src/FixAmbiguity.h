// Shared geometry and acceptance rules for desktop and Android fixes.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace fix_selection {
constexpr double pi = 3.14159265358979323846;
constexpr double rad = pi / 180.0;
struct Position {
  double latitude, longitude;
  Position(double lat = NAN, double lon = NAN)
      : latitude(lat), longitude(lon) {}
};
inline bool Valid(const Position& p) {
  return std::isfinite(p.latitude) && std::isfinite(p.longitude) &&
         std::abs(p.latitude) <= 90 && std::abs(p.longitude) <= 180;
}
inline double DistanceNm(const Position& a, const Position& b) {
  const double x = std::sin((a.latitude - b.latitude) * rad / 2);
  const double y =
      std::sin(std::remainder(a.longitude - b.longitude, 360.0) * rad / 2);
  const double h = std::max(
      0.0, std::min(1.0, x * x + std::cos(a.latitude * rad) *
                                     std::cos(b.latitude * rad) * y * y));
  return 2 * std::atan2(std::sqrt(h), std::sqrt(1 - h)) / rad * 60;
}
struct Circle {
  Position body;
  double altitude;
  Circle(Position p = {}, double alt = NAN) : body(p), altitude(alt) {}
};
struct Intersections {
  std::vector<Position> positions;
  std::string error;
};
inline Intersections Intersect(const Circle& a, const Circle& b) {
  Intersections result;
  if (!Valid(a.body) || !Valid(b.body) || !std::isfinite(a.altitude) ||
      !std::isfinite(b.altitude) || std::abs(a.altitude) > 90 ||
      std::abs(b.altitude) > 90) {
    result.error = "Invalid sight-circle geometry";
    return result;
  }
  auto vector = [](Position p) {
    return std::array<double, 3>{
        std::cos(p.latitude * rad) * std::cos(p.longitude * rad),
        std::cos(p.latitude * rad) * std::sin(p.longitude * rad),
        std::sin(p.latitude * rad)};
  };
  const auto u = vector(a.body), v = vector(b.body);
  std::array<double, 3> n{{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                           u[0] * v[1] - u[1] * v[0]}};
  const double d = u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
  const double determinant = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
  if (determinant < 1e-12) {
    result.error =
        "Sight circles are parallel or ill-conditioned; include another sight";
    return result;
  }
  const double sa = std::sin(a.altitude * rad), sb = std::sin(b.altitude * rad);
  const double c1 = (sa - d * sb) / determinant,
               c2 = (sb - d * sa) / determinant;
  std::array<double, 3> base{
      {c1 * u[0] + c2 * v[0], c1 * u[1] + c2 * v[1], c1 * u[2] + c2 * v[2]}};
  const double height2 =
      1 - (base[0] * base[0] + base[1] * base[1] + base[2] * base[2]);
  if (height2 < -1e-10) {
    result.error =
        "The two sight circles do not intersect; check the observations or "
        "include another sight";
    return result;
  }
  const double height = std::sqrt(std::max(0.0, height2) / determinant);
  for (int sign : {1, -1}) {
    if (sign < 0 && height2 <= 1e-12) break;
    std::array<double, 3> p{{base[0] + sign * height * n[0],
                             base[1] + sign * height * n[1],
                             base[2] + sign * height * n[2]}};
    const double length = std::hypot(p[0], std::hypot(p[1], p[2]));
    for (double& x : p) x /= length;
    // Reject numerical pseudo-intersections rather than silently accepting a
    // clamped solution on impossible geometry.
    if (std::abs(p[0] * u[0] + p[1] * u[1] + p[2] * u[2] - sa) > 1e-9 ||
        std::abs(p[0] * v[0] + p[1] * v[1] + p[2] * v[2] - sb) > 1e-9) {
      result.positions.clear();
      result.error = "Unresolved sight-circle boundary; include another sight";
      return result;
    }
    result.positions.push_back({std::atan2(p[2], std::hypot(p[0], p[1])) / rad,
                                std::atan2(p[1], p[0]) / rad});
  }
  return result;
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
// A changed calculation can never inherit acceptance of a previous candidate.
class Acceptance {
public:
  void SetInputs(const std::string& inputs) {
    if (inputs != inputs_) {
      inputs_ = inputs;
      selected_ = -1;
    }
  }
  void Select(int candidate) { selected_ = candidate; }
  int Selected() const { return selected_; }
  void Clear() { selected_ = -1; }

private:
  std::string inputs_;
  int selected_ = -1;
};
}  // namespace fix_selection
