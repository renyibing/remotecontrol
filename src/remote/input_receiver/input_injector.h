// Abstract interface for receiving side input injection

#ifndef REMOTE_INPUT_RECEIVER_INPUT_INJECTOR_H_
#define REMOTE_INPUT_RECEIVER_INPUT_INJECTOR_H_

#include "remote/proto/messages.h"

namespace remote {
namespace input_receiver {

class IInputInjector {
 public:
  virtual ~IInputInjector() = default;

  // Keyboard injection
  virtual void InjectKeyboard(const proto::KeyboardMsg& ev) = 0;
  // Mouse absolute injection
  virtual void InjectMouseAbs(float x, float y, const proto::Buttons& btns) = 0;
  // Mouse relative injection
  virtual void InjectMouseRel(float dx, float dy, const proto::Buttons& btns) = 0;
  // Wheel
  virtual void InjectWheel(float dx, float dy) = 0;
  // IME state
  virtual void SetIme(const proto::ImeStateMsg& st) = 0;
  // Gamepad
  virtual void InjectGamepad(const proto::GamepadMsg& st) = 0;

  // Optional: directly inject XInput (ViGEm) messages
  // buttons: XUSB wButtons mask; axis range: [-1,1], trigger range: [0,1]
  virtual void InjectGamepadXInput(uint16_t /*buttons*/, float /*lx*/, float /*ly*/, float /*rx*/, float /*ry*/, float /*lt*/, float /*rt*/) {}
};

}  // namespace input_receiver
}  // namespace remote

#endif  // REMOTE_INPUT_RECEIVER_INPUT_INJECTOR_H_
