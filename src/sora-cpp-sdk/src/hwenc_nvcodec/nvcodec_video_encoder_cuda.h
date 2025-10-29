#ifndef SORA_HWENC_NVCODEC_NVCODEC_VIDEO_ENCODER_CUDA_H_
#define SORA_HWENC_NVCODEC_NVCODEC_VIDEO_ENCODER_CUDA_H_

// When mixing CUDA and WebRTC headers, a large number of errors occur, so prepare a simple CUDA file to only do CUDA processing.

#include <memory>

// NvCodec
#include <NvEncoder/NvEncoder.h>

#include "sora/cuda_context.h"

class NvEncoder;

namespace sora {

class NvCodecVideoEncoderCudaImpl;

class NvCodecVideoEncoderCuda {
 public:
  NvCodecVideoEncoderCuda(std::shared_ptr<CudaContext> ctx);
  ~NvCodecVideoEncoderCuda();

  void Copy(NvEncoder* nv_encoder, const void* ptr, int width, int height);
  // For safety, use a pointer without including <memory>.
  NvEncoder* CreateNvEncoder(int width, int height, bool is_nv12);

 private:
  NvCodecVideoEncoderCudaImpl* impl_;
};

}  // namespace sora

#endif
