#ifndef CELESTIAL_ECLIPSE_PCK_H
#define CELESTIAL_ECLIPSE_PCK_H

#include "eclipse/vector.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace eclipse {

// Minimal read-only NAIF binary PCK type-2 reader for the DE440 Moon PA
// orientation kernel. The returned matrix converts ICRF/J2000 vectors to the
// lunar principal-axes body-fixed frame.
class PckKernel {
public:
  struct SegmentInfo {
    double start_et;
    double end_et;
    std::int32_t frame_class_id;
    std::int32_t base_frame;
    std::int32_t type;
    std::int32_t initial_address;
    std::int32_t final_address;
  };

  bool Open(const std::string& path, std::string* error);
  bool IcrfToBodyFixed(double et, Vector3 axes[3], std::string* error) const;
  bool TransformIcrf(double et, const Vector3& icrf, Vector3* body_fixed,
                     std::string* error) const;
  const std::vector<SegmentInfo>& segments() const { return segments_; }

private:
  bool ReadBytes(std::uint64_t offset, void* destination, std::size_t size,
                 std::string* error) const;
  bool ReadDoubleWord(std::int32_t address, double* value,
                      std::string* error) const;

  mutable std::ifstream file_;
  std::string path_;
  std::vector<SegmentInfo> segments_;
};

}  // namespace eclipse

#endif
