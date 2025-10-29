#include "sora/hwenc_vpl/vpl_video_decoder.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

// WebRTC
#include <api/scoped_refptr.h>
#include <api/video/encoded_image.h>
#include <api/video/i420_buffer.h>
#include <api/video/video_codec_type.h>
#include <api/video/video_frame.h>
#include <api/video_codecs/video_decoder.h>
#include <common_video/include/video_frame_buffer_pool.h>
#include <modules/video_coding/include/video_error_codes.h>
#include <rtc_base/logging.h>

// libyuv
#include <libyuv/convert.h>

// Intel VPL
#include <vpl/mfxcommon.h>
#include <vpl/mfxdefs.h>
#include <vpl/mfxstructures.h>
#include <vpl/mfxvideo++.h>
#include <vpl/mfxvideo.h>

#include "../vpl_session_impl.h"
#include "sora/vpl_session.h"
#include "vpl_utils.h"

namespace sora {

class VplVideoDecoderImpl : public VplVideoDecoder {
 public:
  VplVideoDecoderImpl(std::shared_ptr<VplSession> session, mfxU32 codec);
  ~VplVideoDecoderImpl() override;

  bool Configure(const Settings& settings) override;

  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override;

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;

  int32_t Release() override;

  const char* ImplementationName() const override;

  static std::unique_ptr<MFXVideoDECODE> CreateDecoder(
      std::shared_ptr<VplSession> session,
      mfxU32 codec,
      std::vector<std::pair<int, int>> sizes,
      bool init,
      mfxFrameAllocRequest* alloc_request);
  static std::unique_ptr<MFXVideoDECODE> CreateDecoderInternal(
      std::shared_ptr<VplSession> session,
      mfxU32 codec,
      int width,
      int height,
      bool init,
      mfxFrameAllocRequest* alloc_request);

 private:
  bool InitVpl();
  void ReleaseVpl();

  int width_ = 0;
  int height_ = 0;
  webrtc::DecodedImageCallback* decode_complete_callback_ = nullptr;
  webrtc::VideoFrameBufferPool buffer_pool_;

  mfxU32 codec_;
  std::shared_ptr<VplSession> session_;
  mfxFrameAllocRequest alloc_request_;
  std::unique_ptr<MFXVideoDECODE> decoder_;
  std::vector<uint8_t> surface_buffer_;
  std::vector<mfxFrameSurface1> surfaces_;
  std::vector<uint8_t> bitstream_buffer_;
  mfxBitstream bitstream_;
};

VplVideoDecoderImpl::VplVideoDecoderImpl(std::shared_ptr<VplSession> session,
                                         mfxU32 codec)
    : session_(session),
      codec_(codec),
      decoder_(nullptr),
      decode_complete_callback_(nullptr),
      buffer_pool_(false, 300 /* max_number_of_buffers*/) {}

VplVideoDecoderImpl::~VplVideoDecoderImpl() {
  Release();
}

std::unique_ptr<MFXVideoDECODE> VplVideoDecoderImpl::CreateDecoder(
    std::shared_ptr<VplSession> session,
    mfxU32 codec,
    std::vector<std::pair<int, int>> sizes,
    bool init,
    mfxFrameAllocRequest* alloc_request) {
  for (auto size : sizes) {
    memset(alloc_request, 0, sizeof(*alloc_request));
    auto decoder = CreateDecoderInternal(session, codec, size.first,
                                         size.second, init, alloc_request);
    if (decoder != nullptr) {
      return decoder;
    }
  }
  return nullptr;
}

std::unique_ptr<MFXVideoDECODE> VplVideoDecoderImpl::CreateDecoderInternal(
    std::shared_ptr<VplSession> session,
    mfxU32 codec,
    int width,
    int height,
    bool init,
    mfxFrameAllocRequest* alloc_request) {
  std::unique_ptr<MFXVideoDECODE> decoder(
      new MFXVideoDECODE(GetVplSession(session)));

  mfxStatus sts = MFX_ERR_NONE;

  mfxVideoParam param;
  memset(&param, 0, sizeof(param));

  param.mfx.CodecId = codec;

  if (codec == MFX_CODEC_HEVC) {
    // This setting is necessary for the Init of the H.265 decoder to fail with sts=-15.
    param.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
  } else if (codec == MFX_CODEC_AV1) {
    // param.mfx.CodecProfile = MFX_PROFILE_AV1_MAIN;
    // This setting is necessary for the Query to fail with sts=-3.
    // Whether MFX_LEVEL_AV1_2 is appropriate is not clear.
    param.mfx.CodecLevel = MFX_LEVEL_AV1_2;
  }

  param.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
  param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
  param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  param.mfx.FrameInfo.CropX = 0;
  param.mfx.FrameInfo.CropY = 0;
  param.mfx.FrameInfo.CropW = width;
  param.mfx.FrameInfo.CropH = height;
  param.mfx.FrameInfo.Width = (width + 15) / 16 * 16;
  param.mfx.FrameInfo.Height = (height + 15) / 16 * 16;

  param.mfx.GopRefDist = 1;
  param.AsyncDepth = 1;
  param.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

  //qmfxExtCodingOption ext_coding_option;
  //qmemset(&ext_coding_option, 0, sizeof(ext_coding_option));
  //qext_coding_option.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
  //qext_coding_option.Header.BufferSz = sizeof(ext_coding_option);
  //qext_coding_option.MaxDecFrameBuffering = 1;

  //qmfxExtBuffer* ext_buffers[1];
  //qext_buffers[0] = (mfxExtBuffer*)&ext_coding_option;
  //qparam.ExtParam = ext_buffers;
  //qparam.NumExtParam = sizeof(ext_buffers) / sizeof(ext_buffers[0]);

  sts = decoder->Query(&param, &param);
  if (sts < 0) {
    RTC_LOG(LS_VERBOSE) << "Unsupported decoder codec: resolution=" << width
                        << "x" << height << " codec=" << CodecToString(codec)
                        << " sts=" << sts;
    return nullptr;
  }

  if (sts != MFX_ERR_NONE) {
    RTC_LOG(LS_VERBOSE)
        << "Supported specified codec but has warning: resolution=" << width
        << "x" << height << " sts=" << sts;
  }

  // When MFX_CODEC_AV1 is initialized, QueryIOSurf after Init fails with MFX_ERR_UNSUPPORTED, so QueryIOSurf is performed here.
  // (When MFX_CODEC_AVC or MFX_CODEC_HEVC is initialized, QueryIOSurf after Init also works.)
  memset(alloc_request, 0, sizeof(*alloc_request));
  sts = decoder->QueryIOSurf(&param, alloc_request);
  VPL_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

  RTC_LOG(LS_INFO) << "Decoder NumFrameSuggested="
                   << alloc_request->NumFrameSuggested;

  // Sometimes Init after Query fails with MFX_ERR_UNSUPPORTED, so call it always even if Init is not needed.
  /*if (init)*/ {
    // Initialize the Intel VPL encoder
    sts = decoder->Init(&param);
    if (sts != MFX_ERR_NONE) {
      RTC_LOG(LS_VERBOSE) << "Init failed: resolution=" << width << "x"
                          << height << " codec=" << CodecToString(codec)
                          << " sts=" << sts;
      return nullptr;
    }
  }

  return decoder;
}

bool VplVideoDecoderImpl::Configure(
    const webrtc::VideoDecoder::Settings& settings) {
  width_ = settings.max_render_resolution().Width();
  height_ = settings.max_render_resolution().Height();

  return InitVpl();
}

int32_t VplVideoDecoderImpl::Decode(const webrtc::EncodedImage& input_image,
                                    bool missing_frames,
                                    int64_t render_time_ms) {
  if (decoder_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (decode_complete_callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (input_image.data() == nullptr && input_image.size() > 0) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  if (bitstream_.MaxLength < bitstream_.DataLength + input_image.size()) {
    bitstream_buffer_.resize(bitstream_.DataLength + input_image.size());
    bitstream_.MaxLength = bitstream_.DataLength + bitstream_buffer_.size();
    bitstream_.Data = bitstream_buffer_.data();
  }
  //printf("size=%zu\n", input_image.size());
  //for (size_t i = 0; i < input_image.size(); i++) {
  //  const uint8_t* p = input_image.data();
  //  if (i < 100) {
  //    printf(" %02x", p[i]);
  //  } else {
  //    printf("\n");
  //    break;
  //  }
  //}

  memmove(bitstream_.Data, bitstream_.Data + bitstream_.DataOffset,
          bitstream_.DataLength);
  bitstream_.DataOffset = 0;
  memcpy(bitstream_.Data + bitstream_.DataLength, input_image.data(),
         input_image.size());
  bitstream_.DataLength += input_image.size();

  // Get the unused input surface.
  auto surface =
      std::find_if(surfaces_.begin(), surfaces_.end(),
                   [](const mfxFrameSurface1& s) { return !s.Data.Locked; });
  if (surface == surfaces_.end()) {
    RTC_LOG(LS_ERROR) << "Surface not found";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Loop until the queue is empty or sts == MFX_ERR_MORE_DATA.
  while (true) {
    mfxStatus sts;
    mfxSyncPoint syncp;
    mfxFrameSurface1* out_surface = nullptr;

    while (true) {
      sts = decoder_->DecodeFrameAsync(&bitstream_, &*surface, &out_surface,
                                       &syncp);
      if (sts == MFX_WRN_DEVICE_BUSY) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      break;
    }

    mfxVideoParam param;
    memset(&param, 0, sizeof(param));
    mfxStatus sts2 = decoder_->GetVideoParam(&param);
    if (sts2 != MFX_ERR_NONE) {
      RTC_LOG(LS_ERROR) << "Failed to GetVideoParam: sts=" << sts2;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    auto width = param.mfx.FrameInfo.CropW;
    auto height = param.mfx.FrameInfo.CropH;
    if (width_ != width || height_ != height) {
      RTC_LOG(LS_INFO) << "Change Frame Size: " << width_ << "x" << height_
                       << " to " << width << "x" << height;
      width_ = width;
      height_ = height;
    }

    if (sts == MFX_ERR_MORE_DATA) {
      // Since more input is needed, go back.
      return WEBRTC_VIDEO_CODEC_OK;
    }
    if (!syncp) {
      RTC_LOG(LS_WARNING) << "Failed to DecodeFrameAsync: syncp is null, sts="
                          << (int)sts;
      continue;
    }
    VPL_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = MFXVideoCORE_SyncOperation(GetVplSession(session_), syncp, 600000);
    VPL_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    uint64_t pts = input_image.RtpTimestamp();
    // Convert from NV12 to I420.
    webrtc::scoped_refptr<webrtc::I420Buffer> i420_buffer =
        buffer_pool_.CreateI420Buffer(width_, height_);
    libyuv::NV12ToI420(out_surface->Data.Y, out_surface->Data.Pitch,
                       out_surface->Data.UV, out_surface->Data.Pitch,
                       i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                       i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                       i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                       width_, height_);

    webrtc::VideoFrame decoded_image = webrtc::VideoFrame::Builder()
                                           .set_video_frame_buffer(i420_buffer)
                                           .set_timestamp_rtp(pts)
                                           .build();
    decode_complete_callback_->Decoded(decoded_image, std::nullopt,
                                       std::nullopt);
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VplVideoDecoderImpl::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VplVideoDecoderImpl::Release() {
  ReleaseVpl();
  buffer_pool_.Release();
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* VplVideoDecoderImpl::ImplementationName() const {
  return "libvpl";
}

bool VplVideoDecoderImpl::InitVpl() {
  decoder_ = CreateDecoder(session_, codec_, {{4096, 4096}, {2048, 2048}}, true,
                           &alloc_request_);

  mfxStatus sts = MFX_ERR_NONE;

  mfxVideoParam param;
  memset(&param, 0, sizeof(param));
  sts = decoder_->GetVideoParam(&param);
  if (sts != MFX_ERR_NONE) {
    return false;
  }

  // Input bitstream.
  bitstream_buffer_.resize(1024 * 1024);
  memset(&bitstream_, 0, sizeof(bitstream_));
  bitstream_.MaxLength = bitstream_buffer_.size();
  bitstream_.Data = bitstream_buffer_.data();

  // Create the output surfaces for the required number.
  {
    int width = (alloc_request_.Info.Width + 31) / 32 * 32;
    int height = (alloc_request_.Info.Height + 31) / 32 * 32;
    // The number of bytes per surface.
    // Since it is NV12, 1 pixel is 12 bits.
    int size = width * height * 12 / 8;
    surface_buffer_.resize(alloc_request_.NumFrameSuggested * size);

    surfaces_.clear();
    surfaces_.reserve(alloc_request_.NumFrameSuggested);
    for (int i = 0; i < alloc_request_.NumFrameSuggested; i++) {
      mfxFrameSurface1 surface;
      memset(&surface, 0, sizeof(surface));
      surface.Info = param.mfx.FrameInfo;
      surface.Data.Y = surface_buffer_.data() + i * size;
      surface.Data.U = surface_buffer_.data() + i * size + width * height;
      surface.Data.V = surface_buffer_.data() + i * size + width * height + 1;
      surface.Data.Pitch = width;
      surfaces_.push_back(surface);
    }
  }

  return true;
}

void VplVideoDecoderImpl::ReleaseVpl() {
  if (decoder_ != nullptr) {
    decoder_->Close();
  }
  decoder_.reset();
}

////////////////////////
// VplVideoDecoder
////////////////////////

bool VplVideoDecoder::IsSupported(std::shared_ptr<VplSession> session,
                                  webrtc::VideoCodecType codec) {
  if (session == nullptr) {
    return false;
  }

  mfxFrameAllocRequest alloc_request;
  auto decoder = VplVideoDecoderImpl::CreateDecoder(
      session, ToMfxCodec(codec), {{4096, 4096}, {2048, 2048}}, false,
      &alloc_request);

  return decoder != nullptr;
}

std::unique_ptr<VplVideoDecoder> VplVideoDecoder::Create(
    std::shared_ptr<VplSession> session,
    webrtc::VideoCodecType codec) {
  return std::unique_ptr<VplVideoDecoder>(
      new VplVideoDecoderImpl(session, ToMfxCodec(codec)));
}

}  // namespace sora
