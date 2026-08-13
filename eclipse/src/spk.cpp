#include "eclipse/spk.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>

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

double EvaluateChebyshev(const std::vector<double>& coefficients,
                         double argument) {
  if (coefficients.empty()) return 0.0;
  double t0 = 1.0;
  double result = coefficients[0];
  if (coefficients.size() == 1) return result;
  double t1 = argument;
  result += coefficients[1] * t1;
  for (std::size_t index = 2; index < coefficients.size(); ++index) {
    const double next = 2.0 * argument * t1 - t0;
    result += coefficients[index] * next;
    t0 = t1;
    t1 = next;
  }
  return result;
}

}  // namespace

SpkKernel::SpkKernel() {}

bool SpkKernel::ReadBytes(std::uint64_t offset, void* destination,
                          std::size_t size, std::string* error) const {
  file_.clear();
  file_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!file_) {
    if (error) *error = "Unable to seek in SPK file: " + path_;
    return false;
  }
  file_.read(static_cast<char*>(destination),
             static_cast<std::streamsize>(size));
  if (!file_) {
    if (error) *error = "Unexpected end of SPK file: " + path_;
    return false;
  }
  return true;
}

bool SpkKernel::ReadDoubleWord(std::int32_t address, double* value,
                               std::string* error) const {
  if (!value || address < 1) {
    if (error) *error = "Invalid DAF word address";
    return false;
  }
  unsigned char bytes[8];
  if (!ReadBytes(static_cast<std::uint64_t>(address - 1) * 8u, bytes,
                 sizeof(bytes), error)) {
    return false;
  }
  *value = ReadLittleEndian<double>(bytes);
  return true;
}

bool SpkKernel::Open(const std::string& path, std::string* error) {
  file_.close();
  file_.clear();
  segments_.clear();
  path_ = path;
  file_.open(path.c_str(), std::ios::binary);
  if (!file_) {
    if (error) *error = "Unable to open SPK file: " + path;
    return false;
  }

  unsigned char header[1024];
  if (!ReadBytes(0, header, sizeof(header), error)) return false;
  if (std::memcmp(header, "DAF/SPK ", 8) != 0) {
    if (error) *error = "File is not a DAF/SPK kernel: " + path;
    return false;
  }
  if (std::memcmp(header + 88, "LTL-IEEE", 8) != 0) {
    if (error) *error = "Only little-endian IEEE SPK kernels are supported";
    return false;
  }

  const std::int32_t nd = ReadLittleEndian<std::int32_t>(header + 8);
  const std::int32_t ni = ReadLittleEndian<std::int32_t>(header + 12);
  std::int32_t summary_record = ReadLittleEndian<std::int32_t>(header + 76);
  if (nd != 2 || ni != 6 || summary_record < 1) {
    if (error) *error = "Unsupported or corrupt SPK summary layout";
    return false;
  }

  const std::int32_t summary_words = nd + (ni + 1) / 2;
  while (summary_record != 0) {
    unsigned char record[1024];
    if (!ReadBytes(static_cast<std::uint64_t>(summary_record - 1) * 1024u,
                   record, sizeof(record), error)) {
      return false;
    }
    const double next_record_value = ReadLittleEndian<double>(record);
    const double summary_count_value = ReadLittleEndian<double>(record + 16);
    const std::int32_t summary_count =
        static_cast<std::int32_t>(summary_count_value);
    if (summary_count < 0 || summary_count > 125) {
      if (error) *error = "Corrupt SPK summary count";
      return false;
    }

    for (std::int32_t index = 0; index < summary_count; ++index) {
      const unsigned char* summary = record + 24 + index * summary_words * 8;
      SegmentInfo segment;
      segment.start_et = ReadLittleEndian<double>(summary);
      segment.end_et = ReadLittleEndian<double>(summary + 8);
      segment.target = ReadLittleEndian<std::int32_t>(summary + 16);
      segment.center = ReadLittleEndian<std::int32_t>(summary + 20);
      segment.frame = ReadLittleEndian<std::int32_t>(summary + 24);
      segment.type = ReadLittleEndian<std::int32_t>(summary + 28);
      segment.initial_address = ReadLittleEndian<std::int32_t>(summary + 32);
      segment.final_address = ReadLittleEndian<std::int32_t>(summary + 36);
      segment.initial_epoch = 0.0;
      segment.interval_length = 0.0;
      segment.record_size = 0;
      segment.record_count = 0;
      segments_.push_back(segment);
    }
    summary_record = static_cast<std::int32_t>(next_record_value);
  }

  if (segments_.empty()) {
    if (error) *error = "SPK kernel contains no segments";
    return false;
  }
  for (std::size_t index = 0; index < segments_.size(); ++index) {
    SegmentInfo& segment = segments_[index];
    if (segment.type != 2) continue;
    unsigned char directory[32];
    if (!ReadBytes(static_cast<std::uint64_t>(segment.final_address - 4) * 8u,
                   directory, sizeof(directory), error))
      return false;
    segment.initial_epoch = ReadLittleEndian<double>(directory);
    segment.interval_length = ReadLittleEndian<double>(directory + 8);
    const double record_size = ReadLittleEndian<double>(directory + 16);
    const double record_count = ReadLittleEndian<double>(directory + 24);
    segment.record_size = static_cast<std::int32_t>(record_size);
    segment.record_count = static_cast<std::int32_t>(record_count);
    if (segment.interval_length <= 0.0 || segment.record_count <= 0 ||
        segment.record_size < 8 || (segment.record_size - 2) % 3 != 0) {
      if (error) *error = "Corrupt SPK type-2 segment directory";
      return false;
    }
  }
  return true;
}

bool SpkKernel::EvaluateSegment(const SegmentInfo& segment, double et,
                                Vector3* position_km,
                                std::string* error) const {
  if (!position_km) return false;
  if (segment.type != 2) {
    if (error) {
      std::ostringstream stream;
      stream << "Unsupported SPK segment type " << segment.type;
      *error = stream.str();
    }
    return false;
  }
  if (et < segment.start_et || et > segment.end_et) {
    if (error) *error = "Requested time is outside SPK segment coverage";
    return false;
  }

  std::int32_t record_index = static_cast<std::int32_t>(
      std::floor((et - segment.initial_epoch) / segment.interval_length));
  record_index = std::max<std::int32_t>(
      0, std::min<std::int32_t>(segment.record_count - 1, record_index));
  const std::int32_t record_address =
      segment.initial_address + record_index * segment.record_size;

  std::vector<unsigned char> bytes(
      static_cast<std::size_t>(segment.record_size) * 8u);
  if (!ReadBytes(static_cast<std::uint64_t>(record_address - 1) * 8u, &bytes[0],
                 bytes.size(), error))
    return false;
  std::vector<double> record(static_cast<std::size_t>(segment.record_size));
  for (std::int32_t word = 0; word < segment.record_size; ++word)
    record[static_cast<std::size_t>(word)] =
        ReadLittleEndian<double>(&bytes[static_cast<std::size_t>(word) * 8u]);
  const double midpoint = record[0];
  const double radius = record[1];
  if (radius <= 0.0) {
    if (error) *error = "Corrupt SPK type-2 record radius";
    return false;
  }
  const double argument = (et - midpoint) / radius;
  if (argument < -1.0000001 || argument > 1.0000001) {
    if (error) *error = "SPK record selection produced invalid argument";
    return false;
  }

  const std::size_t coefficient_count =
      static_cast<std::size_t>((segment.record_size - 2) / 3);
  std::vector<double> coefficients(coefficient_count);
  double components[3] = {0.0, 0.0, 0.0};
  for (std::size_t component = 0; component < 3; ++component) {
    const std::size_t offset = 2 + component * coefficient_count;
    std::copy(record.begin() + static_cast<std::ptrdiff_t>(offset),
              record.begin() +
                  static_cast<std::ptrdiff_t>(offset + coefficient_count),
              coefficients.begin());
    components[component] = EvaluateChebyshev(coefficients, argument);
  }
  *position_km = Vector3(components[0], components[1], components[2]);
  return true;
}

bool SpkKernel::PositionToSsb(std::int32_t target, double et,
                              Vector3* position_km,
                              std::vector<std::int32_t>* stack,
                              std::string* error) const {
  if (!position_km || !stack) return false;
  if (target == 0) {
    *position_km = Vector3();
    return true;
  }
  if (std::find(stack->begin(), stack->end(), target) != stack->end()) {
    if (error) *error = "Cycle found in SPK target/center graph";
    return false;
  }
  stack->push_back(target);

  const SegmentInfo* selected = NULL;
  for (std::vector<SegmentInfo>::const_reverse_iterator iterator =
           segments_.rbegin();
       iterator != segments_.rend(); ++iterator) {
    if (iterator->target == target && iterator->type == 2 &&
        et >= iterator->start_et && et <= iterator->end_et) {
      selected = &*iterator;
      break;
    }
  }
  if (!selected) {
    if (error) {
      std::ostringstream stream;
      stream << "No supported SPK segment for target " << target;
      *error = stream.str();
    }
    stack->pop_back();
    return false;
  }

  Vector3 relative;
  Vector3 center;
  const bool success =
      EvaluateSegment(*selected, et, &relative, error) &&
      PositionToSsb(selected->center, et, &center, stack, error);
  stack->pop_back();
  if (!success) return false;
  *position_km = center + relative;
  return true;
}

bool SpkKernel::Position(std::int32_t target, std::int32_t center, double et,
                         Vector3* position_km, std::string* error) const {
  if (!file_.is_open()) {
    if (error) *error = "No SPK kernel is open";
    return false;
  }
  if (!position_km) {
    if (error) *error = "Position output pointer is null";
    return false;
  }
  std::vector<std::int32_t> target_stack;
  std::vector<std::int32_t> center_stack;
  Vector3 target_to_ssb;
  Vector3 center_to_ssb;
  if (!PositionToSsb(target, et, &target_to_ssb, &target_stack, error) ||
      !PositionToSsb(center, et, &center_to_ssb, &center_stack, error)) {
    return false;
  }
  *position_km = target_to_ssb - center_to_ssb;
  return true;
}

}  // namespace eclipse
