#ifndef CELESTIAL_ECLIPSE_SPK_H
#define CELESTIAL_ECLIPSE_SPK_H

#include "eclipse/vector.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace eclipse {

// Minimal, read-only NAIF DAF/SPK type-2 reader. DE440s uses type-2 Chebyshev
// position segments, so the runtime does not require CSPICE or network access.
class SpkKernel {
 public:
  struct SegmentInfo {
    double start_et;
    double end_et;
    std::int32_t target;
    std::int32_t center;
    std::int32_t frame;
    std::int32_t type;
    std::int32_t initial_address;
    std::int32_t final_address;
  };

  SpkKernel();

  bool Open(const std::string& path, std::string* error);
  bool IsOpen() const { return file_.is_open(); }
  const std::vector<SegmentInfo>& segments() const { return segments_; }

  // Position of target relative to center in kilometres at TDB seconds past
  // J2000. Supports any pair connected to the SSB by loaded type-2 segments.
  bool Position(std::int32_t target, std::int32_t center, double et,
                Vector3* position_km, std::string* error) const;

 private:
  bool ReadBytes(std::uint64_t offset, void* destination, std::size_t size,
                 std::string* error) const;
  bool ReadDoubleWord(std::int32_t address, double* value,
                      std::string* error) const;
  bool EvaluateSegment(const SegmentInfo& segment, double et,
                       Vector3* position_km, std::string* error) const;
  bool PositionToSsb(std::int32_t target, double et, Vector3* position_km,
                     std::vector<std::int32_t>* stack,
                     std::string* error) const;

  mutable std::ifstream file_;
  std::string path_;
  std::vector<SegmentInfo> segments_;
};

}  // namespace eclipse

#endif

