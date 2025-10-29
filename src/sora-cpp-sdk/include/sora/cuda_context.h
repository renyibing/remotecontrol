#ifndef CUDA_CONTEXT_H_
#define CUDA_CONTEXT_H_

#include <memory>

namespace sora {

// Represent CUDA context without depending on <cuda.h>
class CudaContext {
 public:
  // Create a CUDA context.
  // Return nullptr on platforms that do not support CUDA.
  static std::shared_ptr<CudaContext> Create();
  static bool CanCreate();
};

enum class CudaVideoCodec {
  H264,
  H265,
  VP8,
  VP9,
  AV1,
  JPEG,
};

}  // namespace sora

#endif