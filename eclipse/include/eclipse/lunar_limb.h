#ifndef CELESTIAL_ECLIPSE_LUNAR_LIMB_H
#define CELESTIAL_ECLIPSE_LUNAR_LIMB_H

#include "eclipse/vector.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace eclipse {

struct LunarLimbProfile {
  Vector3 view_direction_body;
  Vector3 image_axis_u_body;
  Vector3 image_axis_v_body;
  std::vector<double> radius_km;

  double RadiusKmForDirection(const Vector3& body_fixed_direction) const;
};

// Compact, offline derivative of a global LOLA radius grid. The source DEM
// is not needed after conversion. Values are signed metre offsets from the
// reference radius in a fixed-size row-major int16 grid.
class LunarLimbGrid {
public:
  LunarLimbGrid();
  bool Open(const std::string& path, std::string* error);
  bool BuildProfile(const Vector3& moon_to_observer_body, int angular_bins,
                    LunarLimbProfile* profile, std::string* error) const;
  bool SupportRadiusKm(const Vector3& limb_direction_body, double* radius_km,
                       std::string* error) const;
  int width() const { return width_; }
  int height() const { return height_; }
  double reference_radius_km() const { return reference_radius_km_; }

private:
  mutable std::ifstream file_;
  std::string path_;
  int width_;
  int height_;
  double reference_radius_km_;
  std::uint64_t data_offset_;
};

bool WriteLunarLimbGrid(const std::string& path, int width, int height,
                        double reference_radius_km,
                        const std::vector<std::int16_t>& metre_offsets,
                        std::string* error);

}  // namespace eclipse

#endif
