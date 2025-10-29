// Description: Serialization wrapper using protobuf (lite)
// - If REMOTE_USE_PROTOBUF is defined, use the generated remote_input.pb.h
// - Otherwise provide empty implementation to compile through

#ifndef REMOTE_PROTO_PROTOBUF_SERIALIZER_H_
#define REMOTE_PROTO_PROTOBUF_SERIALIZER_H_

#include <vector>

#include "remote/proto/messages.h"

#ifdef REMOTE_USE_PROTOBUF
#include "remote/proto/remote_input.pb.h"
#endif

namespace remote {
namespace proto {

#ifdef REMOTE_USE_PROTOBUF

inline std::vector<uint8_t> PbSerializeKeyboard(const KeyboardMsg& k) {
  remote::proto::Envelope env;
  auto* msg = env.mutable_keyboard();
  msg->set_key(k.key);
  msg->set_code(k.code);
  msg->set_down(k.down);
  msg->set_mods(k.mods);
  std::vector<uint8_t> out(env.ByteSizeLong());
  env.SerializeToArray(out.data(), static_cast<int>(out.size()));
  return out;
}

inline std::vector<uint8_t> PbSerializeMouseAbs(const MouseAbsMsg& m) {
  remote::proto::Envelope env;
  auto* msg = env.mutable_mouseabs();
  msg->set_x(m.x);
  msg->set_y(m.y);
  msg->mutable_btns()->set_bits(m.btns.bits);
  msg->set_displayw(m.displayW);
  msg->set_displayh(m.displayH);
  std::vector<uint8_t> out(env.ByteSizeLong());
  env.SerializeToArray(out.data(), static_cast<int>(out.size()));
  return out;
}

inline std::vector<uint8_t> PbSerializeMouseRel(const MouseRelMsg& m) {
  remote::proto::Envelope env;
  auto* msg = env.mutable_mouserel();
  msg->set_dx(m.dx);
  msg->set_dy(m.dy);
  msg->mutable_btns()->set_bits(m.btns.bits);
  msg->set_ratehz(m.rateHz);
  std::vector<uint8_t> out(env.ByteSizeLong());
  env.SerializeToArray(out.data(), static_cast<int>(out.size()));
  return out;
}

inline std::vector<uint8_t> PbSerializeWheel(const MouseWheelMsg& m) {
  remote::proto::Envelope env;
  auto* msg = env.mutable_mousewheel();
  msg->set_dx(m.dx);
  msg->set_dy(m.dy);
  std::vector<uint8_t> out(env.ByteSizeLong());
  env.SerializeToArray(out.data(), static_cast<int>(out.size()));
  return out;
}

inline std::vector<uint8_t> PbSerializeImeState(const ImeStateMsg& im) {
  remote::proto::Envelope env;
  auto* msg = env.mutable_imestate();
  msg->set_open(im.open);
  msg->set_lang(im.lang);
  std::vector<uint8_t> out(env.ByteSizeLong());
  env.SerializeToArray(out.data(), static_cast<int>(out.size()));
  return out;
}

inline std::vector<uint8_t> PbSerializeCursorImage(const CursorImageMsg& ci) {
  remote::proto::Envelope env;
  auto* msg = env.mutable_cursorimage();
  msg->set_w(ci.w);
  msg->set_h(ci.h);
  msg->set_hotspotx(ci.hotspotX);
  msg->set_hotspoty(ci.hotspotY);
  msg->set_visible(ci.visible);
  msg->set_rgba(ci.rgba.data(), static_cast<int>(ci.rgba.size()));
  std::vector<uint8_t> out(env.ByteSizeLong());
  env.SerializeToArray(out.data(), static_cast<int>(out.size()));
  return out;
}

inline std::vector<uint8_t> PbSerializeGamepadXInput(uint16_t buttons, float lx, float ly, float rx, float ry, float lt, float rt) {
  remote::proto::Envelope env;
  auto* msg = env.mutable_gamepadxinput();
  msg->set_buttonsmask(buttons);
  msg->set_lx(lx);
  msg->set_ly(ly);
  msg->set_rx(rx);
  msg->set_ry(ry);
  msg->set_lt(lt);
  msg->set_rt(rt);
  std::vector<uint8_t> out(env.ByteSizeLong());
  env.SerializeToArray(out.data(), static_cast<int>(out.size()));
  return out;
}

#else

// When protobuf is not enabled, return empty bytes, and the caller can fall back to JSON
inline std::vector<uint8_t> PbSerializeKeyboard(const KeyboardMsg&) { return {}; }
inline std::vector<uint8_t> PbSerializeMouseAbs(const MouseAbsMsg&) { return {}; }
inline std::vector<uint8_t> PbSerializeMouseRel(const MouseRelMsg&) { return {}; }
inline std::vector<uint8_t> PbSerializeWheel(const MouseWheelMsg&) { return {}; }
inline std::vector<uint8_t> PbSerializeImeState(const ImeStateMsg&) { return {}; }
inline std::vector<uint8_t> PbSerializeCursorImage(const CursorImageMsg&) { return {}; }
inline std::vector<uint8_t> PbSerializeGamepadXInput(uint16_t, float, float, float, float, float, float) { return {}; }

#endif

}  // namespace proto
}  // namespace remote

#endif  // REMOTE_PROTO_PROTOBUF_SERIALIZER_H_

