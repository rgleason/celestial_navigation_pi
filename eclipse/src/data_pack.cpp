#include "eclipse/data_pack.h"

#include "eclipse/spk.h"
#include "eclipse/pck.h"
#include "eclipse/lunar_limb.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace eclipse {
namespace {

inline std::uint32_t RotateRight(std::uint32_t value, unsigned count) {
  return (value >> count) | (value << (32u - count));
}

class Sha256 {
public:
  Sha256() : bit_count_(0), buffered_(0) {
    const std::uint32_t initial[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
                                      0xa54ff53au, 0x510e527fu, 0x9b05688cu,
                                      0x1f83d9abu, 0x5be0cd19u};
    std::copy(initial, initial + 8, state_);
  }

  void Update(const unsigned char* data, std::size_t size) {
    bit_count_ += static_cast<std::uint64_t>(size) * 8u;
    while (size > 0) {
      const std::size_t take = std::min<std::size_t>(64 - buffered_, size);
      std::memcpy(buffer_ + buffered_, data, take);
      buffered_ += take;
      data += take;
      size -= take;
      if (buffered_ == 64) {
        Transform(buffer_);
        buffered_ = 0;
      }
    }
  }

  std::string Finish() {
    buffer_[buffered_++] = 0x80;
    if (buffered_ > 56) {
      std::memset(buffer_ + buffered_, 0, 64 - buffered_);
      Transform(buffer_);
      buffered_ = 0;
    }
    std::memset(buffer_ + buffered_, 0, 56 - buffered_);
    for (int index = 0; index < 8; ++index) {
      buffer_[63 - index] =
          static_cast<unsigned char>(bit_count_ >> (index * 8));
    }
    Transform(buffer_);
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (int word = 0; word < 8; ++word) output << std::setw(8) << state_[word];
    return output.str();
  }

private:
  void Transform(const unsigned char block[64]) {
    static const std::uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
        0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
        0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
        0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
        0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
        0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
        0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
        0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
        0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
    std::uint32_t words[64];
    for (int index = 0; index < 16; ++index) {
      const int offset = index * 4;
      words[index] = (static_cast<std::uint32_t>(block[offset]) << 24) |
                     (static_cast<std::uint32_t>(block[offset + 1]) << 16) |
                     (static_cast<std::uint32_t>(block[offset + 2]) << 8) |
                     static_cast<std::uint32_t>(block[offset + 3]);
    }
    for (int index = 16; index < 64; ++index) {
      const std::uint32_t s0 = RotateRight(words[index - 15], 7) ^
                               RotateRight(words[index - 15], 18) ^
                               (words[index - 15] >> 3);
      const std::uint32_t s1 = RotateRight(words[index - 2], 17) ^
                               RotateRight(words[index - 2], 19) ^
                               (words[index - 2] >> 10);
      words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }
    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (int index = 0; index < 64; ++index) {
      const std::uint32_t upper =
          RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
      const std::uint32_t choose = (e & f) ^ ((~e) & g);
      const std::uint32_t t1 = h + upper + choose + k[index] + words[index];
      const std::uint32_t lower =
          RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
      const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t t2 = lower + majority;
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  std::uint32_t state_[8];
  std::uint64_t bit_count_;
  unsigned char buffer_[64];
  std::size_t buffered_;
};

DataPackStatus ReadDigest(const std::string& path, const std::string& label) {
  DataPackStatus status;
  std::ifstream input(path.c_str(), std::ios::binary);
  if (!input) {
    status.error = "Unable to open " + label + " data file: " + path;
    return status;
  }
  Sha256 digest;
  unsigned char buffer[1024 * 1024];
  while (input) {
    input.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
    const std::streamsize count = input.gcount();
    if (count > 0) {
      digest.Update(buffer, static_cast<std::size_t>(count));
      status.bytes += static_cast<std::uint64_t>(count);
    }
  }
  if (!input.eof()) {
    status.error = "Error while reading " + label + " data file";
    return status;
  }
  status.sha256 = digest.Finish();
  return status;
}

}  // namespace

const char* ExpectedDe440sSha256() {
  return "c1c7feeab882263fc493a9d5a5b2ddd71b54826cdf65d8d17a76126b260a49f2";
}

std::uint64_t ExpectedDe440sBytes() { return 32726016u; }

DataPackStatus VerifyDe440s(const std::string& path) {
  DataPackStatus status = ReadDigest(path, "DE440s");
  if (!status.error.empty()) return status;
  if (status.bytes != ExpectedDe440sBytes()) {
    std::ostringstream message;
    message << "DE440s size mismatch: expected " << ExpectedDe440sBytes()
            << " bytes, found " << status.bytes;
    status.error = message.str();
    return status;
  }
  if (status.sha256 != ExpectedDe440sSha256()) {
    status.error =
        "DE440s SHA-256 mismatch; the file is corrupt or is not "
        "the supported NAIF kernel";
    return status;
  }
  SpkKernel kernel;
  std::string kernel_error;
  if (!kernel.Open(path, &kernel_error) || kernel.segments().size() != 14u) {
    status.error = kernel_error.empty() ? "Unexpected DE440s segment layout"
                                        : kernel_error;
    return status;
  }
  status.valid = true;
  return status;
}

DataPackStatus VerifyLunarOrientationPck(const std::string& path) {
  DataPackStatus status = ReadDigest(path, "DE440 lunar orientation");
  if (!status.error.empty()) return status;
  const std::uint64_t expected_bytes = 12863488u;
  const char* expected_sha =
      "60cd55aa401ea2ea97360636f567554bfe4e37bb829f901b4460a455dfaf783f";
  if (status.bytes != expected_bytes || status.sha256 != expected_sha) {
    status.error = "Lunar orientation kernel size or SHA-256 mismatch";
    return status;
  }
  PckKernel kernel;
  std::string kernel_error;
  if (!kernel.Open(path, &kernel_error)) {
    status.error = kernel_error;
    return status;
  }
  status.valid = true;
  return status;
}

DataPackStatus VerifyLola64Pa(const std::string& path) {
  DataPackStatus status = ReadDigest(path, "LOLA principal-axes limb");
  if (!status.error.empty()) return status;
  const std::uint64_t expected_bytes = 530841624u;
  const char* expected_sha =
      "f59edf8437442b05525345b3c29b65f0f31af8fc96420abf2dd18af3480f7ff4";
  if (status.bytes != expected_bytes || status.sha256 != expected_sha) {
    status.error = "LOLA limb-pack size or SHA-256 mismatch";
    return status;
  }
  LunarLimbGrid grid;
  std::string grid_error;
  if (!grid.Open(path, &grid_error) || grid.width() != 23040 ||
      grid.height() != 11520) {
    status.error =
        grid_error.empty() ? "Unexpected LOLA limb-pack layout" : grid_error;
    return status;
  }
  status.valid = true;
  return status;
}

}  // namespace eclipse
