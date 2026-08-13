#include "eclipse/lunar_limb.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace eclipse {
namespace {

const char kMagic[8] = {'C', 'L', 'R', 'O', '6', '4', '\r', '\n'};

template <typename T>
bool ReadValue(std::ifstream* input, T* value) {
  input->read(reinterpret_cast<char*>(value), sizeof(T));
  return !!*input;
}

}  // namespace

LunarLimbGrid::LunarLimbGrid()
    : width_(0), height_(0), reference_radius_km_(0.0), data_offset_(0) {}

bool LunarLimbGrid::Open(const std::string& path, std::string* error) {
  file_.close();
  file_.clear();
  path_ = path;
  file_.open(path.c_str(), std::ios::binary);
  if (!file_) {
    if (error) *error = "Unable to open LOLA limb grid: " + path;
    return false;
  }
  char magic[8];
  std::uint32_t width = 0, height = 0;
  if (!file_.read(magic, 8) || std::memcmp(magic, kMagic, 8) != 0 ||
      !ReadValue(&file_, &width) || !ReadValue(&file_, &height) ||
      !ReadValue(&file_, &reference_radius_km_)) {
    if (error) *error = "Invalid LOLA limb-grid header";
    return false;
  }
  if (width < 360 || width > 100000 || height < 180 || height > 50000 ||
      width != height * 2 || reference_radius_km_ < 1700.0 ||
      reference_radius_km_ > 1800.0) {
    if (error) *error = "Unsupported LOLA limb-grid dimensions";
    return false;
  }
  width_ = static_cast<int>(width);
  height_ = static_cast<int>(height);
  data_offset_ = static_cast<std::uint64_t>(file_.tellg());
  file_.seekg(0, std::ios::end);
  const std::uint64_t expected =
      data_offset_ +
      static_cast<std::uint64_t>(width_) * height_ * sizeof(std::int16_t);
  if (static_cast<std::uint64_t>(file_.tellg()) != expected) {
    if (error) *error = "LOLA limb grid is truncated or has trailing data";
    return false;
  }
  return true;
}

double LunarLimbProfile::RadiusKmForDirection(
    const Vector3& body_fixed_direction) const {
  if (radius_km.empty()) return 0.0;
  const double u = Dot(body_fixed_direction, image_axis_u_body);
  const double v = Dot(body_fixed_direction, image_axis_v_body);
  double angle = std::atan2(v, u);
  if (angle < 0.0) angle += 2.0 * 3.14159265358979323846;
  const double position =
      angle * radius_km.size() / (2.0 * 3.14159265358979323846);
  const std::size_t first =
      static_cast<std::size_t>(std::floor(position)) % radius_km.size();
  const std::size_t second = (first + 1) % radius_km.size();
  const double fraction = position - std::floor(position);
  return radius_km[first] * (1.0 - fraction) + radius_km[second] * fraction;
}

bool LunarLimbGrid::BuildProfile(const Vector3& moon_to_observer_body,
                                 int angular_bins, LunarLimbProfile* profile,
                                 std::string* error) const {
  if (!file_ || !profile || angular_bins < 360 || angular_bins > 100000) {
    if (error) *error = "Invalid LOLA limb profile request";
    return false;
  }
  profile->view_direction_body = Normalize(moon_to_observer_body);
  const Vector3 helper = std::fabs(profile->view_direction_body.z) < 0.9
                             ? Vector3(0.0, 0.0, 1.0)
                             : Vector3(0.0, 1.0, 0.0);
  profile->image_axis_u_body =
      Normalize(Cross(helper, profile->view_direction_body));
  profile->image_axis_v_body = Normalize(
      Cross(profile->view_direction_body, profile->image_axis_u_body));
  profile->radius_km.assign(static_cast<std::size_t>(angular_bins),
                            -std::numeric_limits<double>::infinity());

  std::vector<double> cosine_longitude(static_cast<std::size_t>(width_));
  std::vector<double> sine_longitude(static_cast<std::size_t>(width_));
  for (int column = 0; column < width_; ++column) {
    const double longitude =
        2.0 * 3.14159265358979323846 * (column + 0.5) / width_;
    cosine_longitude[column] = std::cos(longitude);
    sine_longitude[column] = std::sin(longitude);
  }
  std::vector<std::int16_t> row(static_cast<std::size_t>(width_));
  file_.clear();
  file_.seekg(static_cast<std::streamoff>(data_offset_), std::ios::beg);
  for (int row_index = 0; row_index < height_; ++row_index) {
    file_.read(reinterpret_cast<char*>(&row[0]),
               static_cast<std::streamsize>(row.size() * sizeof(row[0])));
    if (!file_) {
      if (error) *error = "Error reading LOLA limb grid";
      return false;
    }
    const double latitude =
        3.14159265358979323846 / 2.0 -
        3.14159265358979323846 * (row_index + 0.5) / height_;
    const double cos_latitude = std::cos(latitude);
    const double sin_latitude = std::sin(latitude);
    for (int column = 0; column < width_; ++column) {
      if (row[column] == std::numeric_limits<std::int16_t>::min()) continue;
      const Vector3 normal(cos_latitude * cosine_longitude[column],
                           cos_latitude * sine_longitude[column], sin_latitude);
      // A 0.02-radian belt is over 30 km wide at the lunar surface, safely
      // encompassing all LOLA relief which could form the apparent limb.
      if (std::fabs(Dot(normal, profile->view_direction_body)) > 0.02) continue;
      const double radius = reference_radius_km_ + row[column] / 1000.0;
      const Vector3 point = normal * radius;
      const double u = Dot(point, profile->image_axis_u_body);
      const double v = Dot(point, profile->image_axis_v_body);
      double angle = std::atan2(v, u);
      if (angle < 0.0) angle += 2.0 * 3.14159265358979323846;
      int bin = static_cast<int>(angle * angular_bins /
                                 (2.0 * 3.14159265358979323846));
      bin = std::max(0, std::min(angular_bins - 1, bin));
      profile->radius_km[bin] =
          std::max(profile->radius_km[bin], std::hypot(u, v));
    }
  }
  // Fill rare empty angular bins by nearest-neighbour propagation. At 64 ppd
  // a matching or finer profile normally has none, but this makes coarser
  // synthetic and future grids safe.
  int first_valid = -1;
  for (int index = 0; index < angular_bins; ++index)
    if (std::isfinite(profile->radius_km[index])) {
      first_valid = index;
      break;
    }
  if (first_valid < 0) {
    if (error) *error = "LOLA grid has insufficient data for a limb profile";
    return false;
  }
  double previous = profile->radius_km[first_valid];
  for (int step = 1; step < angular_bins; ++step) {
    const int index = (first_valid + step) % angular_bins;
    if (std::isfinite(profile->radius_km[index]))
      previous = profile->radius_km[index];
    else
      profile->radius_km[index] = previous;
  }
  return true;
}

bool LunarLimbGrid::SupportRadiusKm(const Vector3& limb_direction_body,
                                    double* radius_km,
                                    std::string* error) const {
  if (!file_ || !radius_km || limb_direction_body.Norm() == 0.0) {
    if (error) *error = "Invalid LOLA support-radius request";
    return false;
  }
  const Vector3 direction = Normalize(limb_direction_body);
  const double latitude = std::asin(direction.z);
  double longitude = std::atan2(direction.y, direction.x);
  if (longitude < 0.0) longitude += 2.0 * 3.14159265358979323846;
  const int centre_row =
      static_cast<int>(std::floor((3.14159265358979323846 / 2.0 - latitude) *
                                  height_ / 3.14159265358979323846));
  const int centre_column = static_cast<int>(
      std::floor(longitude * width_ / (2.0 * 3.14159265358979323846)));
  const int row_radius =
      std::max(4, static_cast<int>(std::ceil(2.0 * height_ / 180.0)));
  const int column_radius =
      std::max(4, static_cast<int>(std::ceil(2.0 * width_ / 360.0)));
  std::vector<std::int16_t> row(static_cast<std::size_t>(width_));
  double maximum = -std::numeric_limits<double>::infinity();
  for (int row_index = std::max(0, centre_row - row_radius);
       row_index <= std::min(height_ - 1, centre_row + row_radius);
       ++row_index) {
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(
                    data_offset_ + static_cast<std::uint64_t>(row_index) *
                                       width_ * sizeof(std::int16_t)),
                std::ios::beg);
    file_.read(reinterpret_cast<char*>(&row[0]),
               static_cast<std::streamsize>(row.size() * sizeof(row[0])));
    if (!file_) {
      if (error) *error = "Error reading LOLA support-radius rows";
      return false;
    }
    const double cell_latitude =
        3.14159265358979323846 / 2.0 -
        3.14159265358979323846 * (row_index + 0.5) / height_;
    const double cos_latitude = std::cos(cell_latitude);
    const double sin_latitude = std::sin(cell_latitude);
    for (int offset = -column_radius; offset <= column_radius; ++offset) {
      int column = (centre_column + offset) % width_;
      if (column < 0) column += width_;
      if (row[column] == std::numeric_limits<std::int16_t>::min()) continue;
      const double cell_longitude =
          2.0 * 3.14159265358979323846 * (column + 0.5) / width_;
      const Vector3 normal(cos_latitude * std::cos(cell_longitude),
                           cos_latitude * std::sin(cell_longitude),
                           sin_latitude);
      const double surface_radius = reference_radius_km_ + row[column] / 1000.0;
      maximum = std::max(maximum, surface_radius * Dot(normal, direction));
    }
  }
  if (!std::isfinite(maximum)) {
    if (error) *error = "LOLA grid has no data near the required limb point";
    return false;
  }
  *radius_km = maximum;
  return true;
}

bool WriteLunarLimbGrid(const std::string& path, int width, int height,
                        double reference_radius_km,
                        const std::vector<std::int16_t>& metre_offsets,
                        std::string* error) {
  if (width < 360 || height < 180 || width != height * 2 ||
      metre_offsets.size() != static_cast<std::size_t>(width * height)) {
    if (error) *error = "Invalid source grid for LOLA limb pack";
    return false;
  }
  std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!output) {
    if (error) *error = "Unable to create LOLA limb grid: " + path;
    return false;
  }
  const std::uint32_t width_value = static_cast<std::uint32_t>(width);
  const std::uint32_t height_value = static_cast<std::uint32_t>(height);
  output.write(kMagic, 8);
  output.write(reinterpret_cast<const char*>(&width_value),
               sizeof(width_value));
  output.write(reinterpret_cast<const char*>(&height_value),
               sizeof(height_value));
  output.write(reinterpret_cast<const char*>(&reference_radius_km),
               sizeof(reference_radius_km));
  output.write(reinterpret_cast<const char*>(&metre_offsets[0]),
               static_cast<std::streamsize>(metre_offsets.size() *
                                            sizeof(metre_offsets[0])));
  if (!output) {
    if (error) *error = "Error writing LOLA limb grid";
    return false;
  }
  return true;
}

}  // namespace eclipse
