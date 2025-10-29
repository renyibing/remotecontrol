// Description: Session handshake information skeleton

#ifndef REMOTE_SESSION_HANDSHAKE_H_
#define REMOTE_SESSION_HANDSHAKE_H_

#include <string>

#include "remote/common/geometry.h"

namespace remote {
namespace session {

struct Handshake {
  std::string os;                    // Remote operating system
  bool allowInput{false};            // Whether to allow input injection
  common::Size captureSize{};        // Expected/current capture frame size
  bool gamepadSupported{false};      // Whether to support gamepad injection
};

}  // namespace session
}  // namespace remote

#endif  // REMOTE_SESSION_HANDSHAKE_H_

