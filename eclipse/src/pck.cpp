#include "eclipse/pck.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace eclipse {
namespace {

template <typename T>
T ReadLittleEndian(const unsigned char* data) {
  T value;
  std::memcpy(&value, data, sizeof(T));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  unsigned char* bytes = reinterpret_cast<unsigned char*>(&value);
  std::reverse(bytes, bytes + sizeof(T));
#endif
  return value;
}

double Chebyshev(const std::vector<double>& coefficients, double argument) {
  if (coefficients.empty()) return 0.0;
  double result = coefficients[0];
  if (coefficients.size() == 1) return result;
  double previous = 1.0;
  double current = argument;
  result += coefficients[1] * current;
  for (std::size_t index = 2; index < coefficients.size(); ++index) {
    const double next = 2.0 * argument * current - previous;
    result += coefficients[index] * next;
    previous = current;
    current = next;
  }
  return result;
}

Vector3 RotateZ(const Vector3& value, double angle) {
  const double c = std::cos(angle), s = std::sin(angle);
  return Vector3(c * value.x + s * value.y, -s * value.x + c * value.y,
                 value.z);
}

Vector3 RotateX(const Vector3& value, double angle) {
  const double c = std::cos(angle), s = std::sin(angle);
  return Vector3(value.x, c * value.y + s * value.z,
                 -s * value.y + c * value.z);
}

Vector3 Transform(const Vector3& value, double phi, double delta, double w) {
  // Binary PCK type 2 stores the three Euler angles already prepared as
  // phi, delta and w. SPICE PCKMAT reorders these for a 3-1-3 coordinate
  // transform: R3(w) R1(delta) R3(phi).
  return RotateZ(RotateX(RotateZ(value, phi), delta), w);
}

}  // namespace

bool PckKernel::ReadBytes(std::uint64_t offset, void* destination,
                          std::size_t size, std::string* error) const {
  file_.clear();
  file_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!file_) {
    if (error) *error = "Unable to seek in binary PCK: " + path_;
    return false;
  }
  file_.read(static_cast<char*>(destination),
             static_cast<std::streamsize>(size));
  if (!file_) {
    if (error) *error = "Unexpected end of binary PCK: " + path_;
    return false;
  }
  return true;
}

bool PckKernel::ReadDoubleWord(std::int32_t address, double* value,
                               std::string* error) const {
  unsigned char bytes[8];
  if (!value || address < 1 ||
      !ReadBytes(static_cast<std::uint64_t>(address - 1) * 8u, bytes, 8, error))
    return false;
  *value = ReadLittleEndian<double>(bytes);
  return true;
}

bool PckKernel::Open(const std::string& path, std::string* error) {
  file_.close();
  file_.clear();
  segments_.clear();
  path_ = path;
  file_.open(path.c_str(), std::ios::binary);
  if (!file_) {
    if (error) *error = "Unable to open binary PCK: " + path;
    return false;
  }
  unsigned char header[1024];
  if (!ReadBytes(0, header, sizeof(header), error)) return false;
  if (std::memcmp(header, "DAF/PCK ", 8) != 0 ||
      std::memcmp(header + 88, "LTL-IEEE", 8) != 0 ||
      ReadLittleEndian<std::int32_t>(header + 8) != 2 ||
      ReadLittleEndian<std::int32_t>(header + 12) != 5) {
    if (error) *error = "Unsupported binary PCK layout";
    return false;
  }
  std::int32_t summary_record = ReadLittleEndian<std::int32_t>(header + 76);
  while (summary_record != 0) {
    unsigned char record[1024];
    if (!ReadBytes(static_cast<std::uint64_t>(summary_record - 1) * 1024u,
                   record, sizeof(record), error))
      return false;
    const int count = static_cast<int>(ReadLittleEndian<double>(record + 16));
    if (count < 0 || count > 125) {
      if (error) *error = "Corrupt binary PCK summary count";
      return false;
    }
    for (int index = 0; index < count; ++index) {
      const unsigned char* summary = record + 24 + index * 40;
      SegmentInfo segment;
      segment.start_et = ReadLittleEndian<double>(summary);
      segment.end_et = ReadLittleEndian<double>(summary + 8);
      segment.frame_class_id = ReadLittleEndian<std::int32_t>(summary + 16);
      segment.base_frame = ReadLittleEndian<std::int32_t>(summary + 20);
      segment.type = ReadLittleEndian<std::int32_t>(summary + 24);
      segment.initial_address = ReadLittleEndian<std::int32_t>(summary + 28);
      segment.final_address = ReadLittleEndian<std::int32_t>(summary + 32);
      segments_.push_back(segment);
    }
    summary_record =
        static_cast<std::int32_t>(ReadLittleEndian<double>(record));
  }
  if (segments_.empty()) {
    if (error) *error = "Binary PCK contains no segments";
    return false;
  }
  return true;
}

bool PckKernel::IcrfToBodyFixed(double et, Vector3 axes[3],
                                std::string* error) const {
  if (!axes) return false;
  const SegmentInfo* segment = NULL;
  for (std::vector<SegmentInfo>::const_reverse_iterator iterator =
           segments_.rbegin();
       iterator != segments_.rend(); ++iterator) {
    if (iterator->type == 2 && iterator->base_frame == 1 &&
        et >= iterator->start_et && et <= iterator->end_et) {
      segment = &*iterator;
      break;
    }
  }
  if (!segment) {
    if (error) *error = "No DE440 lunar orientation at requested epoch";
    return false;
  }
  double initial = 0.0, interval = 0.0, record_size_value = 0.0;
  double count_value = 0.0;
  if (!ReadDoubleWord(segment->final_address - 3, &initial, error) ||
      !ReadDoubleWord(segment->final_address - 2, &interval, error) ||
      !ReadDoubleWord(segment->final_address - 1, &record_size_value, error) ||
      !ReadDoubleWord(segment->final_address, &count_value, error))
    return false;
  const int record_size = static_cast<int>(record_size_value);
  const int count = static_cast<int>(count_value);
  if (interval <= 0.0 || count <= 0 || record_size < 8 ||
      (record_size - 2) % 3 != 0) {
    if (error) *error = "Corrupt binary PCK type-2 directory";
    return false;
  }
  int record_index = static_cast<int>(std::floor((et - initial) / interval));
  record_index = std::max(0, std::min(count - 1, record_index));
  const int address = segment->initial_address + record_index * record_size;
  std::vector<double> record(static_cast<std::size_t>(record_size));
  for (int word = 0; word < record_size; ++word)
    if (!ReadDoubleWord(address + word, &record[word], error)) return false;
  const double argument = (et - record[0]) / record[1];
  const std::size_t coefficient_count =
      static_cast<std::size_t>((record_size - 2) / 3);
  double angles[3];
  for (int component = 0; component < 3; ++component) {
    const std::size_t offset = 2 + component * coefficient_count;
    angles[component] = Chebyshev(
        std::vector<double>(
            record.begin() + static_cast<std::ptrdiff_t>(offset),
            record.begin() +
                static_cast<std::ptrdiff_t>(offset + coefficient_count)),
        argument);
  }
  axes[0] = Transform(Vector3(1.0, 0.0, 0.0), angles[0], angles[1], angles[2]);
  axes[1] = Transform(Vector3(0.0, 1.0, 0.0), angles[0], angles[1], angles[2]);
  axes[2] = Transform(Vector3(0.0, 0.0, 1.0), angles[0], angles[1], angles[2]);
  return true;
}

bool PckKernel::TransformIcrf(double et, const Vector3& icrf,
                              Vector3* body_fixed, std::string* error) const {
  if (!body_fixed) return false;
  Vector3 axes[3];
  if (!IcrfToBodyFixed(et, axes, error)) return false;
  *body_fixed = axes[0] * icrf.x + axes[1] * icrf.y + axes[2] * icrf.z;
  return true;
}

}  // namespace eclipse
