// Header-only tool for geometric and coordinate conversion
// Comments in Chinese, emphasizing readability

#ifndef REMOTE_COMMON_GEOMETRY_H_
#define REMOTE_COMMON_GEOMETRY_H_

#include <algorithm>

namespace remote {
namespace common {

// Represents the video drawing rectangle within the SDL on the sending side (window coordinate system)
struct Rect {
  float x{0};
  float y{0};
  float w{0};
  float h{0};
};

// Represents the actual size of the captured frame on the receiving side
struct Size {
  int w{0};
  int h{0};
};

// Map the absolute position of the mouse on the sending side (Mx, My, in window coordinates) to the absolute pixel coordinates on the receiving side
// Returns false if the mouse is not in the video frame drawing area, and should not be sent
inline bool MapMouseAbs(float Mx,
                        float My,
                        const Rect& sdlRect,
                        const Size& recvSize,
                        float& outRx,
                        float& outRy) {
  // Hit test: If the mouse is outside the drawing rectangle, it should not be sent
  if (Mx < sdlRect.x || My < sdlRect.y ||
      Mx >= sdlRect.x + sdlRect.w || My >= sdlRect.y + sdlRect.h) {
    return false;
  }
  // Normalize to [0,1]
  float nx = (Mx - sdlRect.x) / sdlRect.w;
  float ny = (My - sdlRect.y) / sdlRect.h;
  nx = std::clamp(nx, 0.0f, 1.0f);
  ny = std::clamp(ny, 0.0f, 1.0f);
  // Map to the receiving side pixel coordinates
  outRx = nx * static_cast<float>(recvSize.w);
  outRy = ny * static_cast<float>(recvSize.h);
  return true;
}

}  // namespace common
}  // namespace remote

#endif  // REMOTE_COMMON_GEOMETRY_H_

