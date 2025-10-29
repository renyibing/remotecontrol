// SDL event capture and dispatch (sending side)

#ifndef REMOTE_INPUT_SENDER_SDL_INPUT_CAPTURE_H_
#define REMOTE_INPUT_SENDER_SDL_INPUT_CAPTURE_H_

#include <functional>
#include <optional>

#include <SDL3/SDL.h>

#include <iostream>

#include "remote/input_sender/mouse_mapper.h"
#include "remote/proto/messages.h"
#include "remote/proto/serializer.h"
#include "remote/proto/protobuf_serializer.h"

namespace remote {
namespace input_sender {

// Lightweight skeleton: capture SDL events, convert to protocol message callbacks
class SdlInputCapture {
 public:
  // Send function: send the serialized bytes to the DataChannel
  using ReliableSender = std::function<bool(const std::vector<uint8_t>&)>;
  using RtSender = std::function<bool(const std::vector<uint8_t>&)>;

  void SetSenders(ReliableSender reliable, RtSender rt) {
    reliable_ = std::move(reliable);
    rt_ = std::move(rt);
  }

  void UpdateMapping(const common::Rect& sdlRect, const common::Size& recvSize) {
    mapper_.UpdateMapping(sdlRect, recvSize);
  }

  void SetMouseMode(MouseMode mode) { mode_ = mode; }

  // Extract mouse/keyboard from SDL events, assemble protocol messages
  // Current skeleton, serialization/sending carried by the callbacks provided by SetSenders
  void Pump(const SDL_Event& ev) {
    switch (ev.type) {
      case SDL_EVENT_MOUSE_MOTION: {
        // Collect button states
        proto::Buttons btns{};
        btns.bits = ev.motion.state;  // SDL3: button mask
        if (mode_ == MouseMode::Absolute) {
          auto abs = mapper_.MakeAbs(static_cast<float>(ev.motion.x), static_cast<float>(ev.motion.y), btns);
          if (abs) {
            // Use protobuf first, if not available, fall back to JSON
            auto pb = proto::PbSerializeMouseAbs(*abs);
            if (!pb.empty()) { if (reliable_) reliable_(pb); }
            else { auto js = proto::SerializeMouseAbs(*abs); if (reliable_) reliable_(js); }
          } else {
            // Fall back to sending relative displacement, ensuring still controllable
            auto rel = mapper_.MakeRel(static_cast<float>(ev.motion.xrel), static_cast<float>(ev.motion.yrel), btns, 0);
            auto pb = proto::PbSerializeMouseRel(rel);
            if (!pb.empty()) { if (rt_) rt_(pb); }
            else { auto js = proto::SerializeMouseRel(rel); if (rt_) rt_(js); }
          }
        } else {
          auto rel = mapper_.MakeRel(static_cast<float>(ev.motion.xrel), static_cast<float>(ev.motion.yrel), btns, 0);
          auto pb = proto::PbSerializeMouseRel(rel);
          if (!pb.empty()) { if (rt_) rt_(pb); }
          else { auto js = proto::SerializeMouseRel(rel); if (rt_) rt_(js); }
        }
        break;
      }
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP: {
        // Get the current mouse state and coordinates, send a 0 displacement event to synchronize button changes
        float mx = 0.0f, my = 0.0f;
        uint32_t mask = SDL_GetMouseState(&mx, &my);
        proto::Buttons btns{};
        btns.bits = mask;
        if (mode_ == MouseMode::Absolute) {
          auto abs = mapper_.MakeAbs(mx, my, btns);
          if (abs) {
            auto pb = proto::PbSerializeMouseAbs(*abs);
            if (!pb.empty()) { if (reliable_) reliable_(pb); }
            else { auto js = proto::SerializeMouseAbs(*abs); if (reliable_) reliable_(js); }
          } else {
            // Fall back to sending only button states (relative 0,0)
            auto rel = mapper_.MakeRel(0.0f, 0.0f, btns, 0);
            auto pb = proto::PbSerializeMouseRel(rel);
            if (!pb.empty()) { if (rt_) rt_(pb); }
            else { auto js = proto::SerializeMouseRel(rel); if (rt_) rt_(js); }
          }
        } else {
          auto rel = mapper_.MakeRel(0.0f, 0.0f, btns, 0);
          auto pb = proto::PbSerializeMouseRel(rel);
          if (!pb.empty()) { if (rt_) rt_(pb); }
          else { auto js = proto::SerializeMouseRel(rel); if (rt_) rt_(js); }
        }
        break;
      }
      case SDL_EVENT_MOUSE_WHEEL: {
        proto::MouseWheelMsg wh{};
        wh.dx = static_cast<float>(ev.wheel.x);
        wh.dy = static_cast<float>(ev.wheel.y);
        auto pb = proto::PbSerializeWheel(wh);
        if (!pb.empty()) { if (reliable_) reliable_(pb); }
        else { auto js = proto::SerializeWheel(wh); if (reliable_) reliable_(js); }
        break;
      }
      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP: {
        proto::KeyboardMsg k{};
        const bool down = (ev.type == SDL_EVENT_KEY_DOWN);
        k.down = down;
        k.code = static_cast<int>(ev.key.key);
        // Get the human-readable name from SDL
        const char* name = SDL_GetKeyName(ev.key.key);
        if (name) k.key = name; else k.key = "";
        // Modifier key state
        k.mods = static_cast<proto::ModBits>(SDL_GetModState());

        if (ev.key.key == SDLK_UP || ev.key.key == SDLK_DOWN ||
            ev.key.key == SDLK_LEFT || ev.key.key == SDLK_RIGHT) {
          std::cout << "[input] SDL capture key=" << k.key
                    << " code=" << k.code
                    << " down=" << (k.down ? "true" : "false")
                    << " mods=" << k.mods << std::endl;
        }

        auto pb = proto::PbSerializeKeyboard(k);
        if (!pb.empty()) { if (reliable_) reliable_(pb); }
        else { auto js = proto::SerializeKeyboard(k); if (reliable_) reliable_(js); }
        break;
      }
      default:
        break;
    }
  }

 private:
  MouseMode mode_{MouseMode::Absolute};
  MouseMapper mapper_{};
  ReliableSender reliable_{};
  RtSender rt_{};
};

}  // namespace input_sender
}  // namespace remote

#endif  // REMOTE_INPUT_SENDER_SDL_INPUT_CAPTURE_H_
