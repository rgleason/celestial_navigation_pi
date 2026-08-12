#ifndef CELESTIAL_ECLIPSE_DATA_PACK_H
#define CELESTIAL_ECLIPSE_DATA_PACK_H

#include <cstdint>
#include <string>

namespace eclipse {

struct DataPackStatus {
  bool valid;
  std::uint64_t bytes;
  std::string sha256;
  std::string error;

  DataPackStatus() : valid(false), bytes(0) {}
};

// The official NAIF de440s kernel is the compact 1849-2150 subset used by
// this module. Verification is deliberately local and performs no downloads.
const char* ExpectedDe440sSha256();
std::uint64_t ExpectedDe440sBytes();
DataPackStatus VerifyDe440s(const std::string& path);

}  // namespace eclipse

#endif
