#ifndef SORA_VPL_SESSION_H_
#define SORA_VPL_SESSION_H_

#include <memory>

namespace sora {

struct VplSession {
  // Create an Intel VPL session.
  // Return nullptr if not supported or an error occurs.
  static std::shared_ptr<VplSession> Create();
};

}  // namespace sora

#endif
