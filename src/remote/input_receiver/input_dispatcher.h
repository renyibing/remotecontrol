// Input message dispatcher, parses DataChannel text JSON messages and injects them into IInputInjector,
// and updates the sending side overlay (cursor image/IME, etc.).

#ifndef REMOTE_INPUT_RECEIVER_INPUT_DISPATCHER_H_
#define REMOTE_INPUT_RECEIVER_INPUT_DISPATCHER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <rtc_base/logging.h>

#include "remote/proto/messages.h"
#include "remote/proto/parser.h"
#include "remote/overlay/overlay_renderer.h"
#include "remote/input_receiver/input_injector.h"
#include <SDL3/SDL_keycode.h>

#include <iostream>
#ifdef REMOTE_USE_PROTOBUF
#include "remote/proto/remote_input.pb.h"
#endif
#ifdef _WIN32
#include <windows.h>
#endif

namespace remote {
namespace input_receiver {

// Empty implementation injector: default does nothing, as a placeholder
class NullInputInjector : public IInputInjector {
 public:
  void InjectKeyboard(const proto::KeyboardMsg&) override {}
  void InjectMouseAbs(float, float, const proto::Buttons&) override {}
  void InjectMouseRel(float, float, const proto::Buttons&) override {}
  void InjectWheel(float, float) override {}
  void SetIme(const proto::ImeStateMsg&) override {}
  void InjectGamepad(const proto::GamepadMsg&) override {}
};

// Simple JSON numerical parsing (float)
inline std::optional<float> JsonGetFloat(std::string_view s, std::string_view key) {
  std::string pattern = std::string("\"") + std::string(key) + std::string("\":");
  size_t p = s.find(pattern);
  if (p == std::string::npos) return std::nullopt;
  p += pattern.size();
  size_t q = p;
  auto isnum = [](char c) {
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E';
  };
  while (q < s.size() && isnum(s[q])) q++;
  if (q == p) return std::nullopt;
  return std::stof(std::string(s.substr(p, q - p)));
}

class InputDispatcher {
 public:
  // inj cannot be empty; overlay can be empty
  InputDispatcher(IInputInjector* inj, overlay::OverlayRenderer* overlay)
      : injector_(inj), overlay_(overlay) {}

  // Process data received from DataChannel (expected to be JSON text)
  void OnMessage(const uint8_t* data, size_t len) {
    if (len == 0 || data == nullptr) return;
    std::string_view sv(reinterpret_cast<const char*>(data), len);
    auto type = proto::JsonGetType(sv);
    if (!type) return;
    if (*type == "cursorImage") {
      if (overlay_) {
        proto::CursorImageMsg ci;
        if (proto::ParseCursorImage(sv, ci)) {
          overlay_->SetCursorImage(ci);
        }
      }
      return;
    }
    if (*type == "imeState") {
      proto::ImeStateMsg im;
      if (proto::ParseImeState(sv, im)) {
        injector_->SetIme(im);
        if (overlay_) { overlay_->SetImeState(im); }
      }
      return;
    }
    if (*type == "keyboard") {
      proto::KeyboardMsg k{};
      if (auto code = proto::JsonGetInt(sv, "code")) k.code = *code;
      if (auto down = proto::JsonGetBool(sv, "down")) k.down = *down;
      if (auto mods = proto::JsonGetInt(sv, "mods")) k.mods = static_cast<proto::ModBits>(*mods);
      if (auto key = proto::JsonGetString(sv, "key")) k.key = *key;
      if (k.code == SDLK_UP || k.code == SDLK_DOWN ||
          k.code == SDLK_LEFT || k.code == SDLK_RIGHT) {
        // std::cout << "[input] DataChannel recv key=" << k.key
                  // << " code=" << k.code
                  // << " down=" << (k.down ? "true" : "false")
                  // << " mods=" << k.mods << std::endl;
      }
      injector_->InjectKeyboard(k);
      return;
    }
    if (*type == "mouseAbs") {
      proto::Buttons b{};
      if (auto bits = proto::JsonGetInt(sv, "buttons")) b.bits = static_cast<uint32_t>(*bits);
      float x = JsonGetFloat(sv, "x").value_or(0.0f);
      float y = JsonGetFloat(sv, "y").value_or(0.0f);
      int dw = proto::JsonGetInt(sv, "displayW").value_or(0);
      int dh = proto::JsonGetInt(sv, "displayH").value_or(0);
#ifdef _WIN32
      if (dw > 0 && dh > 0) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        if (sw > 0 && sh > 0) {
          x = x * (float)sw / (float)dw;
          y = y * (float)sh / (float)dh;
        }
      }
#endif
      injector_->InjectMouseAbs(x, y, b);
      return;
    }
    if (*type == "mouseRel") {
      proto::Buttons b{};
      if (auto bits = proto::JsonGetInt(sv, "buttons")) b.bits = static_cast<uint32_t>(*bits);
      float dx = JsonGetFloat(sv, "dx").value_or(0.0f);
      float dy = JsonGetFloat(sv, "dy").value_or(0.0f);
      injector_->InjectMouseRel(dx, dy, b);
      return;
    }
    if (*type == "mouseWheel") {
      float dx = JsonGetFloat(sv, "dx").value_or(0.0f);
      float dy = JsonGetFloat(sv, "dy").value_or(0.0f);
      injector_->InjectWheel(dx, dy);
      return;
    }
    if (*type == "gamepadXInput") {
      // Parse XInput controller state and inject directly
      uint16_t buttons = static_cast<uint16_t>(proto::JsonGetInt(sv, "buttonsMask").value_or(0));
      float lx = JsonGetFloat(sv, "lx").value_or(0.0f);
      float ly = JsonGetFloat(sv, "ly").value_or(0.0f);
      float rx = JsonGetFloat(sv, "rx").value_or(0.0f);
      float ry = JsonGetFloat(sv, "ry").value_or(0.0f);
      float lt = JsonGetFloat(sv, "lt").value_or(0.0f);
      float rt = JsonGetFloat(sv, "rt").value_or(0.0f);
      injector_->InjectGamepadXInput(buttons, lx, ly, rx, ry, lt, rt);
      return;
    }
    // Other types to be handled later (gamepad/touch/uiCmd, etc.)
  }

  // Same entry: select the parsing path based on the binary flag
  void OnMessageEither(const uint8_t* data, size_t len, bool is_binary) {
#ifdef REMOTE_USE_PROTOBUF
    if (is_binary) {
      // Try to parse protobuf Envelope
      ParseProtoEnvelope(data, len);
      return;
    }
#endif
    // Text JSON
    OnMessage(data, len);
  }

#ifdef REMOTE_USE_PROTOBUF
  void ParseProtoEnvelope(const uint8_t* data, size_t len) {
    remote::proto::Envelope env;
    if (!env.ParseFromArray(data, static_cast<int>(len))) return;
    switch (env.payload_case()) {
      case remote::proto::Envelope::kKeyboard: {
        const auto& m = env.keyboard();
        proto::KeyboardMsg k{m.key(), m.code(), m.down(), static_cast<proto::ModBits>(m.mods())};
        injector_->InjectKeyboard(k);
        break;
      }
      case remote::proto::Envelope::kMouseAbs: {
        const auto& m = env.mouseabs();
        proto::Buttons b{m.btns().bits()};
        float x = m.x();
        float y = m.y();
#ifdef _WIN32
        if (m.displayw() > 0 && m.displayh() > 0) {
          int sw = GetSystemMetrics(SM_CXSCREEN);
          int sh = GetSystemMetrics(SM_CYSCREEN);
          if (sw > 0 && sh > 0) {
            x = x * (float)sw / (float)m.displayw();
            y = y * (float)sh / (float)m.displayh();
          }
        }
#endif
        injector_->InjectMouseAbs(x, y, b);
        break;
      }
      case remote::proto::Envelope::kMouseRel: {
        const auto& m = env.mouserel();
        proto::Buttons b{m.btns().bits()};
        injector_->InjectMouseRel(m.dx(), m.dy(), b);
        break;
      }
      case remote::proto::Envelope::kMouseWheel: {
        const auto& m = env.mousewheel();
        injector_->InjectWheel(m.dx(), m.dy());
        break;
      }
      case remote::proto::Envelope::kCursorImage: {
        if (!overlay_) break;
        const auto& m = env.cursorimage();
        proto::CursorImageMsg ci;
        ci.w = m.w(); ci.h = m.h(); ci.hotspotX = m.hotspotx(); ci.hotspotY = m.hotspoty(); ci.visible = m.visible();
        ci.rgba.assign(m.rgba().begin(), m.rgba().end());
        overlay_->SetCursorImage(ci);
        break;
      }
      case remote::proto::Envelope::kImeState: {
        const auto& m = env.imestate();
        proto::ImeStateMsg st; st.open = m.open(); st.lang = m.lang();
        injector_->SetIme(st);
        if (overlay_) { overlay_->SetImeState(st); }
        break;
      }
      case remote::proto::Envelope::kGamepadXInput: {
        const auto& m = env.gamepadxinput();
        injector_->InjectGamepadXInput(static_cast<uint16_t>(m.buttonsmask()), m.lx(), m.ly(), m.rx(), m.ry(), m.lt(), m.rt());
        break;
      }
      default:
        break;
    }
  }
#endif

 private:
  IInputInjector* injector_;
  overlay::OverlayRenderer* overlay_;
};

}  // namespace input_receiver
}  // namespace remote

#endif  // REMOTE_INPUT_RECEIVER_INPUT_DISPATCHER_H_
