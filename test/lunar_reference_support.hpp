#pragma once
#include "LunarDistanceEngine.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <random>
namespace ld = lunar_distance;
namespace {
constexpr double pi = 3.14159265358979323846;
using Row = std::array<double, 7>;
std::vector<std::string> Fields(const std::string& s) {
  std::istringstream in(s);
  std::vector<std::string> v;
  std::string x;
  while (std::getline(in, x, '\t')) v.push_back(x);
  return v;
}
struct Case {
  std::string name, body, path;
  ld::GeographicPoint observer;
};

std::ofstream NoiseEvidence(const char* name) {
  const char* directory = std::getenv("CELNAV_LUNAR_EVIDENCE_DIR");
  if (!directory) return {};
  std::ofstream out(std::string(directory) + "/" + name);
  EXPECT_TRUE(out.is_open());
  out << std::setprecision(17);
  return out;
}

void RecordNoise(std::ofstream& out, const Case& c, int trial,
                 const ld::Observation& o, const ld::SolveResult& r) {
  if (!out.is_open()) return;
  out << "{\"case\":" << std::quoted(c.name) << ",\"trial\":" << trial
      << ",\"raw_distance_deg\":" << o.raw_distance_deg
      << ",\"moon_altitude_deg\":" << o.moon_altitude_deg
      << ",\"body_altitude_deg\":" << o.body_altitude_deg
      << ",\"valid\":" << (r.valid ? "true" : "false")
      << ",\"error\":" << std::quoted(r.error)
      << ",\"closest_offset_seconds\":" << r.closest_offset_seconds
      << ",\"closest_residual_arcmin\":" << r.closest_residual_arcmin
      << ",\"candidates\":[";
  bool comma = false;
  for (const auto& candidate : r.candidates) {
    if (comma) out << ',';
    comma = true;
    out << "{\"seconds\":" << candidate.offset_seconds
        << ",\"uncertainty_available\":"
        << (candidate.uncertainty_available ? "true" : "false")
        << ",\"positions\":[";
    bool pc = false;
    for (const auto& p : candidate.positions) {
      if (pc) out << ',';
      pc = true;
      out << '[' << p.latitude_deg << ',' << p.longitude_deg << ']';
    }
    out << "]}";
  }
  out << "]}\n";
}
std::vector<Case> Cases() {
  std::ifstream in(std::string(LUNAR_REFERENCE_DIR) +
                   "/utc-consistent/cases.tsv");
  std::string line;
  std::getline(in, line);
  std::vector<Case> result;
  while (std::getline(in, line)) {
    auto f = Fields(line);
    result.push_back({f[0], f[2], f[6], {std::stod(f[3]), std::stod(f[4])}});
  }
  return result;
}
ld::EphemerisFunction Reference(const Case& c, bool observer_callback = false) {
  std::ifstream in(std::string(LUNAR_REFERENCE_DIR) + "/../" + c.path);
  std::string line;
  std::getline(in, line);
  std::vector<Row> rows;
  while (std::getline(in, line)) {
    auto f = Fields(line);
    Row r{};
    for (int i = 0; i < 7; ++i) r[i] = std::stod(f[i]);
    rows.push_back(r);
  }
  EXPECT_GT(rows.size(), 2u);
  return [rows, c, observer_callback](double t, ld::EphemerisSample* out,
                                      std::string* error) {
    if (rows.size() < 2 || t < rows.front()[0] || t > rows.back()[0]) {
      if (error) *error = "reference range";
      return false;
    }
    int j = std::min(
        int(rows.size()) - 2,
        int(std::upper_bound(rows.begin(), rows.end(), t,
                             [](double x, const Row& r) { return x < r[0]; }) -
            rows.begin()) -
            1);
    Row r{};
    double q = (t - rows[j][0]) / (rows[j + 1][0] - rows[j][0]);
    for (int k = 0; k < 7; ++k)
      r[k] = rows[j][k] +
             q * ((k == 1 || k == 4)
                      ? std::remainder(rows[j + 1][k] - rows[j][k], 360)
                      : rows[j + 1][k] - rows[j][k]);
    ld::EphemerisSample s;
    s.moon_geographic_latitude_deg = r[2];
    s.moon_geographic_longitude_deg = -r[1];
    s.body_geographic_latitude_deg = r[5];
    s.body_geographic_longitude_deg = -r[4];
    s.predicted_distance_deg =
        ld::GreatCircleDistanceNm({r[2], -r[1]}, {r[5], -r[4]}) / 60;
    s.moon_horizontal_parallax_deg = std::asin(6378.137 / r[3]) * 180 / pi;
    s.moon_semidiameter_deg = std::asin(1737.4 / r[3]) * 180 / pi;
    if (c.body == "Sun") {
      s.body_horizontal_parallax_deg = std::asin(6378.137 / r[6]) * 180 / pi;
      s.body_semidiameter_deg = std::asin(695700 / r[6]) * 180 / pi;
    }
    // Independent ECEF/local-vertical implementation exercises the public
    // observer callback seam; it is not the plugin's actual provider dispatch.
    if (observer_callback)
      s.observer_direction = [r, c](double lat, double lon, double height,
                                    bool moon, double* alt, double* az,
                                    double* sd) {
        lat *= pi / 180;
        lon *= pi / 180;
        double gp_lat = (moon ? r[2] : r[5]) * pi / 180,
               gp_lon = -(moon ? r[1] : r[4]) * pi / 180;
        std::array<double, 3> direction{std::cos(gp_lat) * std::cos(gp_lon),
                                        std::cos(gp_lat) * std::sin(gp_lon),
                                        std::sin(gp_lat)};
        const double distance = moon ? r[3] : (c.body == "Sun" ? r[6] : 0);
        const double radius = moon ? 1737.4 : (c.body == "Sun" ? 695700 : 0);
        if (distance > 0) {
          constexpr double a = 6378.137, f = 1 / 298.257223563,
                           e2 = f * (2 - f);
          double n = a / std::sqrt(1 - e2 * std::sin(lat) * std::sin(lat));
          double h = height / 1000;
          std::array<double, 3> station{(n + h) * std::cos(lat) * std::cos(lon),
                                        (n + h) * std::cos(lat) * std::sin(lon),
                                        (n * (1 - e2) + h) * std::sin(lat)};
          for (int k = 0; k < 3; ++k)
            direction[k] = distance * direction[k] - station[k];
        }
        const double norm =
            std::hypot(std::hypot(direction[0], direction[1]), direction[2]);
        for (auto& x : direction) x /= norm;
        const double up = direction[0] * std::cos(lat) * std::cos(lon) +
                          direction[1] * std::cos(lat) * std::sin(lon) +
                          direction[2] * std::sin(lat);
        const double east =
            -direction[0] * std::sin(lon) + direction[1] * std::cos(lon);
        const double north = -direction[0] * std::sin(lat) * std::cos(lon) -
                             direction[1] * std::sin(lat) * std::sin(lon) +
                             direction[2] * std::cos(lat);
        *alt = std::asin(std::clamp(up, -1., 1.)) * 180 / pi;
        *az = std::atan2(east, north) * 180 / pi;
        *sd = distance > 0 ? std::asin(radius / norm) * 180 / pi : 0;
        return std::isfinite(*alt) && std::isfinite(*sd);
      };
    *out = s;
    return true;
  };
}
ld::Observation Settings(const Case& c, bool tagged = false) {
  ld::Observation o;
  o.use_ellipsoid = true;
  o.separate_times = tagged;
  o.eye_height_m = 2;
  o.index_error_arcmin = .8;
  o.temperature_c = 15;
  o.pressure_hpa = 1013;
  o.moon_altitude_limb = ld::AltitudeLimb::Lower;
  o.body_altitude_limb =
      c.body == "Sun" ? ld::AltitudeLimb::Upper : ld::AltitudeLimb::Center;
  o.moon_contact = ld::DistanceContact::Near;
  o.body_contact =
      c.body == "Sun" ? ld::DistanceContact::Far : ld::DistanceContact::Center;
  if (tagged) {
    o.moon_time_offset_seconds = -120;
    o.body_time_offset_seconds = 120;
  }
  return o;
}
ld::Observation Generate(ld::Observation o, const ld::EphemerisFunction& p,
                         double t, const ld::GeographicPoint& position) {
  auto f = ld::PredictTimeTaggedObservation(o, p, t, position);
  EXPECT_TRUE(f.valid) << f.error;
  o.raw_distance_deg = f.raw_distance_deg;
  o.moon_altitude_deg = f.moon_altitude_deg;
  o.body_altitude_deg = f.body_altitude_deg;
  return o;
}
ld::SolveOptions Options(double step) {
  ld::SolveOptions o;
  o.start_offset_seconds = -119.63;
  o.end_offset_seconds = 120.37;
  o.scan_step_seconds = step;
  o.root_tolerance_seconds = .0005;
  return o;
}
bool Recovers(const ld::SolveResult& r, double t,
              const ld::GeographicPoint& truth) {
  for (const auto& c : r.candidates)
    if (std::fabs(c.offset_seconds - t) < .0005)
      for (const auto& p : c.positions)
        if (ld::GreatCircleDistanceNm(p, truth) < .001) return true;
  return false;
}
}  // namespace
