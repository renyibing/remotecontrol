// Mouse position conversion and message generator (sending side)

#ifndef REMOTE_INPUT_SENDER_MOUSE_MAPPER_H_
#define REMOTE_INPUT_SENDER_MOUSE_MAPPER_H_

#include <optional>

#include "remote/common/geometry.h"
#include "remote/proto/messages.h"

namespace remote {
namespace input_sender {

enum class MouseMode { Absolute, Relative };

class MouseMapper {
 public:
  // Synchronize the sending side SDL drawing rectangle and the receiving side frame size
  void UpdateMapping(common::Rect sdlRect, common::Size recvSize) {
    sdlRect_ = sdlRect;
    recvSize_ = recvSize;
  }

  // Generate absolute coordinate message; if the mouse is not in the video rectangle, return std::nullopt
  std::optional<proto::MouseAbsMsg> MakeAbs(float Mx, float My, const proto::Buttons& btns) const {
    float rx = 0, ry = 0;
    if (!common::MapMouseAbs(Mx, My, sdlRect_, recvSize_, rx, ry)) {
      return std::nullopt;
    }
    proto::MouseAbsMsg m;
    m.x = rx;
    m.y = ry;
    m.btns = btns;
    m.displayW = recvSize_.w;
    m.displayH = recvSize_.h;
    return m;
  }

  // Generate relative coordinate message
  proto::MouseRelMsg MakeRel(float dx, float dy, const proto::Buttons& btns, int rate_hz = 0) const {
    proto::MouseRelMsg m;
    m.dx = dx;
    m.dy = dy;
    m.btns = btns;
    m.rateHz = rate_hz;
    return m;
  }

 private:
  common::Rect sdlRect_{};
  common::Size recvSize_{};
};

}  // namespace input_sender
}  // namespace remote

#endif  // REMOTE_INPUT_SENDER_MOUSE_MAPPER_H_

