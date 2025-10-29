// Description: Simple JSON serialization (initial version)
// Comments in Chinese, emphasizing readability

#ifndef REMOTE_PROTO_SERIALIZER_H_
#define REMOTE_PROTO_SERIALIZER_H_

#include <sstream>
#include <vector>
#include <string>

#include "remote/proto/messages.h"
#include "remote/proto/xusb.h"

namespace remote {
namespace proto {

inline std::vector<uint8_t> SerializeMouseAbs(const MouseAbsMsg& m) {
  std::ostringstream os;
  os.setf(std::ios::fixed);
  os.precision(3);
  os << "{\"type\":\"mouseAbs\","
        "\"x\":" << m.x << ",\"y\":" << m.y
     << ",\"buttons\":" << m.btns.bits
     << ",\"displayW\":" << m.displayW
     << ",\"displayH\":" << m.displayH
     << "}";
  auto s = os.str();
  return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> SerializeMouseRel(const MouseRelMsg& m) {
  std::ostringstream os;
  os.setf(std::ios::fixed);
  os.precision(3);
  os << "{\"type\":\"mouseRel\"," 
        "\"dx\":" << m.dx << ",\"dy\":" << m.dy
     << ",\"buttons\":" << m.btns.bits
     << ",\"rateHz\":" << m.rateHz
     << "}";
  auto s = os.str();
  return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> SerializeWheel(const MouseWheelMsg& m) {
  std::ostringstream os;
  os.setf(std::ios::fixed);
  os.precision(3);
  os << "{\"type\":\"mouseWheel\"," 
        "\"dx\":" << m.dx << ",\"dy\":" << m.dy
     << "}";
  auto s = os.str();
  return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> SerializeKeyboard(const KeyboardMsg& k) {
  std::ostringstream os;
  os << "{\"type\":\"keyboard\",";
  // Note: key may contain spaces, use rough escape processing
  std::string key = k.key;
  for (auto& ch : key) {
    if (ch == '\\' || ch == '"') ch = ' ';
  }
  os << "\"key\":\"" << key << "\",";
  os << "\"code\":" << k.code << ",";
  os << "\"down\":" << (k.down ? "true" : "false") << ",";
  os << "\"mods\":" << k.mods << "}";
  auto s = os.str();
  return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> SerializeImeState(const ImeStateMsg& im) {
  std::ostringstream os;
  os << "{\"type\":\"imeState\",";
  os << "\"open\":" << (im.open ? "true" : "false") << ",";
  os << "\"lang\":\"" << im.lang << "\"}";
  auto s = os.str();
  return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> SerializeGamepadXInput(uint16_t buttons, float lx, float ly, float rx, float ry, float lt, float rt) {
  std::ostringstream os;
  os.setf(std::ios::fixed); os.precision(3);
  os << "{\"type\":\"gamepadXInput\",";
  os << "\"buttonsMask\":" << buttons << ",";
  os << "\"lx\":" << lx << ",\"ly\":" << ly << ",";
  os << "\"rx\":" << rx << ",\"ry\":" << ry << ",";
  os << "\"lt\":" << lt << ",\"rt\":" << rt << "}";
  auto s = os.str();
  return std::vector<uint8_t>(s.begin(), s.end());
}

}  // namespace proto
}  // namespace remote

#endif  // REMOTE_PROTO_SERIALIZER_H_
