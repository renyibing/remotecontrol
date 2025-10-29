// Description: Remote input protocol message (initial simplified as structs, serialization replaced later)
// Comments in Chinese, emphasizing readability

#ifndef REMOTE_PROTO_MESSAGES_H_
#define REMOTE_PROTO_MESSAGES_H_

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace remote {
namespace proto {

// Bit flag/mapping type
using ModBits = uint32_t;  // Modifier key mask

struct Buttons {
  // Simple bit mask representation of mouse/gamepad buttons; initially only placeholder
  uint32_t bits{0};
};

// Keyboard event
struct KeyboardMsg {
  std::string key;  // Logical key name, e.g. "KeyA"
  int code{0};      // Platform scan code or unified code
  bool down{false}; // Pressed/released
  ModBits mods{0};  // Modifier key
};

// Mouse absolute position event
struct MouseAbsMsg {
  float x{0};
  float y{0};
  Buttons btns{};
  int displayW{0};
  int displayH{0};
};

// Mouse relative position event
struct MouseRelMsg {
  float dx{0};
  float dy{0};
  Buttons btns{};
  int rateHz{0};
};

// Wheel
struct MouseWheelMsg {
  float dx{0};
  float dy{0};
};

// Remote mouse image
struct CursorImageMsg {
  int w{0};
  int h{0};
  int hotspotX{0};
  int hotspotY{0};
  bool visible{false};
  std::vector<uint8_t> rgba;  // ARGB or RGBA
};

// IME state
struct ImeStateMsg {
  bool open{false};
  std::string lang;  // e.g. zh-CN, en-US, ja-JP
};

// Gamepad (simplified)
struct GamepadMsg {
  std::string profile;                               // xbox/ps4
  std::unordered_map<std::string, bool> buttons;     // A/B/X/Y etc.
  std::unordered_map<std::string, float> axes;       // LX/LY/RX/RY etc.
};

}  // namespace proto
}  // namespace remote

#endif  // REMOTE_PROTO_MESSAGES_H_
