#ifndef VIDEO_CODEC_INFO_H_
#define VIDEO_CODEC_INFO_H_

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#if defined(USE_NVCODEC_ENCODER)
#include "sora/hwenc_nvcodec/nvcodec_video_decoder.h"
#include "sora/hwenc_nvcodec/nvcodec_video_encoder.h"
#endif

#if defined(USE_VPL_ENCODER)
#include "sora/hwenc_vpl/vpl_video_decoder.h"
#include "sora/hwenc_vpl/vpl_video_encoder.h"
#endif

#if defined(USE_JETSON_ENCODER)
#include "sora/hwenc_jetson/jetson_video_decoder.h"
#include "sora/hwenc_jetson/jetson_video_encoder.h"
#endif

struct VideoCodecInfo {
  enum class Type {
    Default,
    Jetson,
    NVIDIA,
    Intel,
    VideoToolbox,
    V4L2,
    Software,
    NotSupported,
  };

  std::vector<Type> vp8_encoders;
  std::vector<Type> vp8_decoders;

  std::vector<Type> vp9_encoders;
  std::vector<Type> vp9_decoders;

  std::vector<Type> av1_encoders;
  std::vector<Type> av1_decoders;

  std::vector<Type> h264_encoders;
  std::vector<Type> h264_decoders;

  std::vector<Type> h265_encoders;
  std::vector<Type> h265_decoders;

  // Resolve the Default and make it a proper encoder name or NotSupported
  static Type Resolve(Type specified, const std::vector<Type>& codecs) {
    if (codecs.empty()) {
      return Type::NotSupported;
    }
    if (specified == Type::Default) {
      // Use the first one
      return codecs[0];
    }
    auto it = std::find(codecs.begin(), codecs.end(), specified);
    if (it != codecs.end()) {
      return *it;
    }
    return Type::NotSupported;
  }

  static std::vector<std::pair<std::string, VideoCodecInfo::Type>>
  GetValidMappingInfo(std::vector<Type> types) {
    std::vector<std::pair<std::string, VideoCodecInfo::Type>> infos;
    infos.push_back({"default", VideoCodecInfo::Type::Default});
    for (auto type : types) {
      auto p = TypeToString(type);
      infos.push_back({p.second, type});
    }
    return infos;
  }

  static std::pair<std::string, std::string> TypeToString(Type type) {
    switch (type) {
      case Type::Jetson:
        return {"Jetson", "jetson"};
      case Type::NVIDIA:
        return {"NVIDIA VIDEO CODEC SDK", "nvidia"};
      case Type::Intel:
        return {"Intel VPL", "vpl"};
      case Type::VideoToolbox:
        return {"VideoToolbox", "videotoolbox"};
      case Type::V4L2:
        return {"V4L2", "v4l2"};
      case Type::Software:
        return {"Software", "software"};
      default:
        return {"Unknown", "unknown"};
    }
  }

  static VideoCodecInfo Get() {
#if defined(_WIN32)
    return GetWindows();
#elif defined(__APPLE__)
    return GetMacos();
#elif defined(__linux__)
    return GetLinux();
#endif
  }

 private:
#if defined(_WIN32)

  static VideoCodecInfo GetWindows() {
    VideoCodecInfo info;

#if defined(USE_NVCODEC_ENCODER)
    auto cuda_context = sora::CudaContext::Create();
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::VP8)) {
      info.vp8_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::VP9)) {
      info.vp9_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::AV1)) {
      info.av1_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::H264)) {
      info.h264_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::H265)) {
      info.h265_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::VP8)) {
      info.vp8_decoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::VP9)) {
      info.vp9_decoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::AV1)) {
      info.av1_decoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::H264)) {
      info.h264_decoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::H265)) {
      info.h265_decoders.push_back(Type::NVIDIA);
    }
#endif

#if defined(USE_VPL_ENCODER)
    auto session = sora::VplSession::Create();
    if (session != nullptr) {
      if (sora::VplVideoEncoder::IsSupported(session, webrtc::kVideoCodecVP8)) {
        info.vp8_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoEncoder::IsSupported(session, webrtc::kVideoCodecVP9)) {
        info.vp9_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoEncoder::IsSupported(session,
                                             webrtc::kVideoCodecH264)) {
        info.h264_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoEncoder::IsSupported(session,
                                             webrtc::kVideoCodecH265)) {
        info.h265_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoEncoder::IsSupported(session, webrtc::kVideoCodecAV1)) {
        info.av1_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session, webrtc::kVideoCodecVP8)) {
        info.vp8_decoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session, webrtc::kVideoCodecVP9)) {
        info.vp9_decoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session,
                                             webrtc::kVideoCodecH264)) {
        info.h264_decoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session,
                                             webrtc::kVideoCodecH265)) {
        info.h265_decoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session, webrtc::kVideoCodecAV1)) {
        info.av1_decoders.push_back(Type::Intel);
      }
    }
#endif

    info.vp8_encoders.push_back(Type::Software);
    info.vp8_decoders.push_back(Type::Software);
    info.vp9_encoders.push_back(Type::Software);
    info.vp9_decoders.push_back(Type::Software);
    info.av1_encoders.push_back(Type::Software);
    info.av1_decoders.push_back(Type::Software);
    info.h264_encoders.push_back(Type::Software);

    return info;
  }

#elif defined(__APPLE__)

  static VideoCodecInfo GetMacos() {
    VideoCodecInfo info;

    info.h264_encoders.push_back(Type::VideoToolbox);
    info.h264_decoders.push_back(Type::VideoToolbox);
    info.h265_encoders.push_back(Type::VideoToolbox);
    info.h265_decoders.push_back(Type::VideoToolbox);
    info.vp8_encoders.push_back(Type::Software);
    info.vp9_encoders.push_back(Type::Software);
    info.vp8_decoders.push_back(Type::Software);
    info.vp9_decoders.push_back(Type::Software);
    info.av1_encoders.push_back(Type::Software);
    info.av1_decoders.push_back(Type::Software);
    info.h264_encoders.push_back(Type::Software);

    return info;
  }

#elif defined(__linux__)

  static VideoCodecInfo GetLinux() {
    VideoCodecInfo info;

#if defined(USE_NVCODEC_ENCODER)
    auto cuda_context = sora::CudaContext::Create();
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::VP8)) {
      info.vp8_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::VP9)) {
      info.vp9_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::AV1)) {
      info.av1_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::H264)) {
      info.h264_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoEncoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::H265)) {
      info.h265_encoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::VP8)) {
      info.vp8_decoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::VP9)) {
      info.vp9_decoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::AV1)) {
      info.av1_decoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::H264)) {
      info.h264_decoders.push_back(Type::NVIDIA);
    }
    if (sora::NvCodecVideoDecoder::IsSupported(cuda_context,
                                               sora::CudaVideoCodec::H265)) {
      info.h265_decoders.push_back(Type::NVIDIA);
    }
#endif

#if defined(USE_VPL_ENCODER)
    auto session = sora::VplSession::Create();
    if (session != nullptr) {
      if (sora::VplVideoEncoder::IsSupported(session, webrtc::kVideoCodecVP8)) {
        info.vp8_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoEncoder::IsSupported(session, webrtc::kVideoCodecVP9)) {
        info.vp9_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoEncoder::IsSupported(session,
                                             webrtc::kVideoCodecH264)) {
        info.h264_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoEncoder::IsSupported(session,
                                             webrtc::kVideoCodecH265)) {
        info.h265_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoEncoder::IsSupported(session, webrtc::kVideoCodecAV1)) {
        info.av1_encoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session, webrtc::kVideoCodecVP8)) {
        info.vp8_decoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session, webrtc::kVideoCodecVP9)) {
        info.vp9_decoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session,
                                             webrtc::kVideoCodecH264)) {
        info.h264_decoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session,
                                             webrtc::kVideoCodecH265)) {
        info.h265_decoders.push_back(Type::Intel);
      }
      if (sora::VplVideoDecoder::IsSupported(session, webrtc::kVideoCodecAV1)) {
        info.av1_decoders.push_back(Type::Intel);
      }
    }
#endif

#if defined(USE_JETSON_ENCODER)
    if (sora::JetsonVideoEncoder::IsSupported(webrtc::kVideoCodecH264)) {
      info.h264_encoders.push_back(Type::Jetson);
    }
    if (sora::JetsonVideoEncoder::IsSupported(webrtc::kVideoCodecH265)) {
      info.h265_encoders.push_back(Type::Jetson);
    }
    if (sora::JetsonVideoEncoder::IsSupported(webrtc::kVideoCodecVP8)) {
      info.vp8_encoders.push_back(Type::Jetson);
    }
    if (sora::JetsonVideoEncoder::IsSupported(webrtc::kVideoCodecVP9)) {
      info.vp9_encoders.push_back(Type::Jetson);
    }
    if (sora::JetsonVideoEncoder::IsSupported(webrtc::kVideoCodecAV1)) {
      info.av1_encoders.push_back(Type::Jetson);
    }
    if (sora::JetsonVideoDecoder::IsSupported(webrtc::kVideoCodecH264)) {
      info.h264_decoders.push_back(Type::Jetson);
    }
    if (sora::JetsonVideoDecoder::IsSupported(webrtc::kVideoCodecH265)) {
      info.h265_decoders.push_back(Type::Jetson);
    }
    if (sora::JetsonVideoDecoder::IsSupported(webrtc::kVideoCodecVP8)) {
      info.vp8_decoders.push_back(Type::Jetson);
    }
    if (sora::JetsonVideoDecoder::IsSupported(webrtc::kVideoCodecVP9)) {
      info.vp9_decoders.push_back(Type::Jetson);
    }
    if (sora::JetsonVideoDecoder::IsSupported(webrtc::kVideoCodecAV1)) {
      info.av1_decoders.push_back(Type::Jetson);
    }
#endif

#if defined(USE_V4L2_ENCODER)
    info.h264_encoders.push_back(Type::V4L2);
    info.h264_decoders.push_back(Type::V4L2);
#endif

    info.vp8_encoders.push_back(Type::Software);
    info.vp8_decoders.push_back(Type::Software);
    info.vp9_encoders.push_back(Type::Software);
    info.vp9_decoders.push_back(Type::Software);
    info.av1_encoders.push_back(Type::Software);
    info.av1_decoders.push_back(Type::Software);
    info.h264_encoders.push_back(Type::Software);

    return info;
  }

#endif
};

#endif  // VIDEO_CODEC_INFO_H_
