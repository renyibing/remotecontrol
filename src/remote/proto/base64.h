// Description: Minimal Base64 decoding (used to parse image data in JSON)
// Comments in Chinese, emphasizing readability

#ifndef REMOTE_PROTO_BASE64_H_
#define REMOTE_PROTO_BASE64_H_

#include <string>
#include <vector>

namespace remote {
namespace proto {

inline int B64Val(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

inline std::vector<uint8_t> Base64Decode(const std::string& in) {
  std::vector<uint8_t> out;
  out.reserve(in.size() * 3 / 4);
  int val = 0;
  int valb = -8;
  for (unsigned char c : in) {
    if (c == '=') break;
    int d = B64Val(c);
    if (d < 0) continue;
    val = (val << 6) | d;
    valb += 6;
    if (valb >= 0) {
      out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

inline std::string Base64Encode(const std::vector<uint8_t>& bytes) {
  static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((bytes.size() + 2) / 3 * 4);
  size_t i = 0;
  while (i + 3 <= bytes.size()) {
    uint32_t n = (bytes[i] << 16) | (bytes[i+1] << 8) | bytes[i+2];
    out.push_back(B64[(n >> 18) & 63]);
    out.push_back(B64[(n >> 12) & 63]);
    out.push_back(B64[(n >> 6) & 63]);
    out.push_back(B64[n & 63]);
    i += 3;
  }
  if (i + 1 == bytes.size()) {
    uint32_t n = (bytes[i] << 16);
    out.push_back(B64[(n >> 18) & 63]);
    out.push_back(B64[(n >> 12) & 63]);
    out.push_back('=');
    out.push_back('=');
  } else if (i + 2 == bytes.size()) {
    uint32_t n = (bytes[i] << 16) | (bytes[i+1] << 8);
    out.push_back(B64[(n >> 18) & 63]);
    out.push_back(B64[(n >> 12) & 63]);
    out.push_back(B64[(n >> 6) & 63]);
    out.push_back('=');
  }
  return out;
}

}  // namespace proto
}  // namespace remote

#endif  // REMOTE_PROTO_BASE64_H_
