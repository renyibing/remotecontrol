#ifndef MOMO_ARGS_H_
#define MOMO_ARGS_H_

#include <iostream>
#include <string>

// Boost
#include <boost/json.hpp>

// WebRTC
#include <api/rtp_parameters.h>
// #include <rtc_base/crypt_string_revive.h>
#include <rtc_base/proxy_info_revive.h>

#include "video_codec_info.h"

struct MomoArgs {
  bool no_google_stun = false;
  bool no_video_device = false;
  bool no_audio_device = false;
  bool list_devices = false;
#if defined(USE_FAKE_CAPTURE_DEVICE)
  bool fake_capture_device = false;
#endif
  bool force_i420 = false;
  bool force_yuy2 = false;
  bool force_nv12 = false;
  // Default true only on Jetson
#if defined(USE_JETSON_ENCODER)
  bool hw_mjpeg_decoder = true;
#else
  bool hw_mjpeg_decoder = false;
#endif
  // Only available on Raspberry Pi OS 64-bit
  bool use_libcamera = false;
  // Only available when use_libcamera == true.
  // Only works when sora_video_codec_type == "H264" and sora_simulcast == false.
  bool use_libcamera_native = false;
  // Libcamera control settings specified as key value pairs.
  std::vector<std::pair<std::string, std::string>> libcamera_controls;
  std::string video_device = "";
  std::string resolution = "VGA";
  int framerate = 30;
  bool fixed_resolution = false;
  std::string priority = "FRAMERATE";
  bool use_sdl = false;
  int window_width = 640;
  int window_height = 480;
  bool fullscreen = false;
  bool low_latency = false;
  std::string serial_device = "";
  unsigned int serial_rate = 9600;
  bool insecure = false;
  bool screen_capture = false;
  bool screen_capture_cursor = false;
  int metrics_port = -1;
  bool metrics_allow_external_ip = false;
  std::string client_cert;
  std::string client_key;

  std::vector<std::string> sora_signaling_urls;
  std::string sora_channel_id;
  bool sora_video = true;
  bool sora_audio = true;
  // If empty, codec will be chosen by Sora
  std::string sora_video_codec_type = "";
  std::string sora_audio_codec_type = "";
  // If 0, bit rate will be chosen by Sora
  int sora_video_bit_rate = 0;
  int sora_audio_bit_rate = 0;
  bool sora_auto_connect = false;
  boost::json::value sora_metadata;
  // sendonly, recvonly, sendrecv
  std::string sora_role = "sendonly";
  bool sora_spotlight = false;
  int sora_spotlight_number = 0;
  int sora_port = -1;
  bool sora_simulcast = false;
  boost::optional<bool> sora_data_channel_signaling;
  int sora_data_channel_signaling_timeout = 180;
  boost::optional<bool> sora_ignore_disconnect_websocket;
  int sora_disconnect_wait_timeout = 5;

  std::string p2p_document_root;
  int p2p_port = 8080;

  std::string ayame_signaling_url;
  std::string ayame_room_id;
  std::string ayame_client_id = "";
  std::string ayame_signaling_key = "";
  // sendrecv, sendonly, recvonly
  std::string ayame_direction = "sendrecv";
  // If empty, use WebRTC default codec
  std::string ayame_video_codec_type = "";
  std::string ayame_audio_codec_type = "";

  bool disable_echo_cancellation = false;
  bool disable_auto_gain_control = false;
  bool disable_noise_suppression = false;
  bool disable_highpass_filter = false;

  // Select the audio output device (initial specified for dynamic switching) for Windows
  int audio_output_device_index = -1;    // -1 is not specified
  std::string audio_output_device_guid;  // Empty string is not specified

  VideoCodecInfo::Type vp8_encoder = VideoCodecInfo::Type::Default;
  VideoCodecInfo::Type vp8_decoder = VideoCodecInfo::Type::Default;
  VideoCodecInfo::Type vp9_encoder = VideoCodecInfo::Type::Default;
  VideoCodecInfo::Type vp9_decoder = VideoCodecInfo::Type::Default;
  VideoCodecInfo::Type av1_encoder = VideoCodecInfo::Type::Default;
  VideoCodecInfo::Type av1_decoder = VideoCodecInfo::Type::Default;
  VideoCodecInfo::Type h264_encoder = VideoCodecInfo::Type::Default;
  VideoCodecInfo::Type h264_decoder = VideoCodecInfo::Type::Default;
  VideoCodecInfo::Type h265_encoder = VideoCodecInfo::Type::Default;
  VideoCodecInfo::Type h265_decoder = VideoCodecInfo::Type::Default;

  std::string openh264;

  std::string proxy_url;
  std::string proxy_username;
  std::string proxy_password;

  // Specify congestion control algorithm (GCC or SQP). Default is "GCC".
  // If SQP is unsupported, the implementation will fall back to GCC.
  std::string congestion_controller = "GCC";  // GCC / SQP

  struct Size {
    int width;
    int height;
  };
  Size GetSize() {
    if (resolution == "QVGA") {
      return {320, 240};
    } else if (resolution == "VGA") {
      return {640, 480};
    } else if (resolution == "HD") {
      return {1280, 720};
    } else if (resolution == "FHD") {
      return {1920, 1080};
    } else if (resolution == "4K") {
      return {3840, 2160};
    }

    // Format like 128x96
    auto pos = resolution.find('x');
    if (pos == std::string::npos) {
      return {16, 16};
    }
    auto width = std::atoi(resolution.substr(0, pos).c_str());
    auto height = std::atoi(resolution.substr(pos + 1).c_str());
    return {std::max(16, width), std::max(16, height)};
  }
};

#endif
