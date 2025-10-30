#include <atomic>
#include <condition_variable>
#include <csignal>
#include <iostream>
// Remote control framework (sender overlay / input capture / data channel management)
#include <string>
#include <thread>
// Handling of --list-devices option
#include <vector>

// SDL3
#include <SDL3/SDL_main.h>

// WebRTC
#include <api/make_ref_counted.h>
#include <rtc_base/log_sinks.h>
#include <rtc_base/ref_counted_object.h>
#include <rtc_base/string_utils.h>

#if defined(USE_SCREEN_CAPTURER)
#include "rtc/screen_video_capturer.h"
// If NvCodec is enabled and a HW MJPEG decoder is used, CUDA must be available
#endif

#if defined(__APPLE__)
#include "mac_helper/mac_capturer.h"
#elif defined(__linux__)
#if defined(USE_JETSON_ENCODER)
#include "sora/hwenc_jetson/jetson_v4l2_capturer.h"
#elif defined(USE_NVCODEC_ENCODER)
#include "sora/hwenc_nvcodec/nvcodec_v4l2_capturer.h"
// If --fake-capture-device is specified, use FakeVideoCapturer
#elif defined(USE_V4L2_ENCODER)
#include "sora/hwenc_v4l2/libcamera_capturer.h"
#include "sora/hwenc_v4l2/v4l2_capturer.h"
#endif
#include "sora/v4l2/v4l2_video_capturer.h"
#else
#include "rtc/device_video_capturer.h"
#endif

#if defined(USE_FAKE_CAPTURE_DEVICE)
#include "rtc/fake_audio_capturer.h"
#include "rtc/fake_video_capturer.h"
#endif
// Even if use_libcamera_native == true, do not output native frames when using simulcast

#include "serial_data_channel/serial_data_manager.h"

#include "sdl_renderer/sdl_renderer.h"

// Remote control framework (sender overlay / input capture / data channel management)
#include "remote/common/geometry.h"
#include "remote/data_channel/input_data_manager.h"
#include "remote/input_receiver/input_dispatcher.h"
#include "remote/input_sender/sdl_input_capture.h"
#include "remote/overlay/overlay_renderer.h"
#include "remote/proto/parser.h"

#ifdef _WIN32
#include "remote/platform/windows/cursor_monitor_win.h"
#include "remote/platform/windows/ime_monitor_win.h"
#include "remote/platform/windows/input_injector_win.h"
#endif

#include "ayame/ayame_client.h"
#include "metrics/metrics_server.h"
// Instantiate overlay and input capture framework
#include "p2p/p2p_server.h"
#include "rtc/rtc_manager.h"
#include "sora/sora_client.h"
#include "sora/sora_server.h"
#include "util.h"

#if defined(__linux__)
#include "sora/v4l2/v4l2_device.h"
// Called after video frame rendering to composite mouse/keyboard/gamepad/RDP toolbar
#endif

#ifdef _WIN32
#include <rtc_base/win/scoped_com_initializer.h>
// Register event hook: overlay handles events first (virtual keyboard/toolbar);
// if not consumed, dispatch to input capture
#endif

#if defined(USE_NVCODEC_ENCODER)
#include "sora/cuda_context.h"
#endif

const size_t kDefaultMaxLogFileSize = 10 * 1024 * 1024;
// Update mapping from current primary video rect to receiver (source) frame size

#if defined(__linux__)

static void ListVideoDevices() {
  auto devices = sora::EnumV4L2CaptureDevices();
  if (!devices) {
    std::cerr << "Failed to enumerate video devices" << std::endl;
    return;
  }

  // Do not intercept initially; return false to let SDL continue processing
  std::cout << "=== Available video devices ===" << std::endl;
  std::cout << std::endl;
  std::cout << sora::FormatV4L2Devices(*devices);
}

// Add input DataChannel manager framework (input-reliable / input-rt)
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
  webrtc::ScopedCOMInitializer com_initializer(
      webrtc::ScopedCOMInitializer::kMTA);
  // Receiver (use_sdl=true): only for displaying IME/cursor; do not perform local injection
  if (!com_initializer.Succeeded()) {
    std::cerr << "CoInitializeEx failed" << std::endl;
    return 1;
  }
#endif

  MomoArgs args;

  bool use_p2p = false;
  bool use_ayame = false;
  // Sender (use_sdl=false): receive control messages and inject locally; report IME/cursor
  bool use_sora = false;
  int log_level = webrtc::LS_NONE;

  Util::ParseArgs(argc, argv, use_p2p, use_ayame, use_sora, log_level, args);

#if defined(__linux__)
  // Processing of --list-devices option
  if (args.list_devices) {
    ListVideoDevices();
    return 0;
  }
#else
  if (args.list_devices) {
    std::cerr << "--list-devices is only supported on Linux" << std::endl;
    return 1;
  }
  // IME/cursor reporting enabled only for sender
#endif

  webrtc::LogMessage::LogToDebug((webrtc::LoggingSeverity)log_level);
  webrtc::LogMessage::LogTimestamps();
  webrtc::LogMessage::LogThreads();

  std::unique_ptr<webrtc::FileRotatingLogSink> log_sink(
      new webrtc::FileRotatingLogSink("./", "webrtc_logs",
                                      kDefaultMaxLogFileSize, 10));
  if (!log_sink->Init()) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << "Failed to open log file";
    log_sink.reset();
    return 1;
  }
  webrtc::LogMessage::AddLogToStream(log_sink.get(), webrtc::LS_INFO);

#if defined(USE_NVCODEC_ENCODER)
  auto cuda_context = sora::CudaContext::Create();

  // If NvCodec is enabled and a HW MJPEG decoder is used, CUDA must be available
  if (args.hw_mjpeg_decoder && cuda_context == nullptr) {
    std::cerr << "Specified --hw-mjpeg-decoder=true but CUDA is invalid."
              << std::endl;
    return 2;
  }
#endif

  auto capturer =
      ([&]() -> webrtc::scoped_refptr<sora::ScalableVideoTrackSource> {
        if (args.no_video_device) {
          return nullptr;
        }

#if defined(USE_FAKE_CAPTURE_DEVICE)
        // If --fake-capture-device is specified, use FakeVideoCapturer
        if (args.fake_capture_device) {
          auto size = args.GetSize();
          FakeVideoCapturer::Config video_config;
          video_config.width = size.width;
          video_config.height = size.height;
          video_config.fps = args.framerate;
          video_config.force_nv12 = args.force_nv12;
          return FakeVideoCapturer::Create(video_config);
        }
#endif

#if defined(USE_SCREEN_CAPTURER)
        if (args.screen_capture) {
          RTC_LOG(LS_INFO) << "Screen capturer source list: "
                           << ScreenVideoCapturer::GetSourceListString();
          webrtc::DesktopCapturer::SourceList sources;
          if (!ScreenVideoCapturer::GetSourceList(&sources)) {
            RTC_LOG(LS_ERROR) << __FUNCTION__ << "Failed select screen source";
            return nullptr;
          }
          auto size = args.GetSize();
          return webrtc::make_ref_counted<ScreenVideoCapturer>(
              sources[0].id, size.width, size.height, args.framerate,
              args.screen_capture_cursor);
        }
#endif

        auto size = args.GetSize();
#if defined(__APPLE__)
        return MacCapturer::Create(size.width, size.height, args.framerate,
                                   args.video_device);
#elif defined(__linux__)
        sora::V4L2VideoCapturerConfig v4l2_config;
        v4l2_config.video_device = args.video_device;
        v4l2_config.width = size.width;
        v4l2_config.height = size.height;
        v4l2_config.framerate = args.framerate;
        v4l2_config.force_i420 = args.force_i420;
        v4l2_config.force_yuy2 = args.force_yuy2;
        v4l2_config.force_nv12 = args.force_nv12;
        v4l2_config.use_native = args.hw_mjpeg_decoder;

#if defined(USE_JETSON_ENCODER)
        if (v4l2_config.use_native) {
          return sora::JetsonV4L2Capturer::Create(std::move(v4l2_config));
        } else {
          return sora::V4L2VideoCapturer::Create(std::move(v4l2_config));
        }
#elif defined(USE_NVCODEC_ENCODER)
        if (v4l2_config.use_native) {
          sora::NvCodecV4L2CapturerConfig nvcodec_config = v4l2_config;
          nvcodec_config.cuda_context = cuda_context;
          return sora::NvCodecV4L2Capturer::Create(std::move(nvcodec_config));
        } else {
          return sora::V4L2VideoCapturer::Create(std::move(v4l2_config));
        }
#elif defined(USE_V4L2_ENCODER)
        if (args.use_libcamera) {
          sora::LibcameraCapturerConfig libcamera_config = v4l2_config;
          // If use_libcamera_native == true, do not output native frames when using simulcast
          libcamera_config.native_frame_output =
              args.use_libcamera_native && !(use_sora && args.sora_simulcast);
          libcamera_config.controls = args.libcamera_controls;
          return sora::LibcameraCapturer::Create(libcamera_config);
        } else if (v4l2_config.use_native &&
                   !(use_sora && args.sora_simulcast)) {
          return sora::V4L2Capturer::Create(std::move(v4l2_config));
        } else {
          return sora::V4L2VideoCapturer::Create(std::move(v4l2_config));
        }
#else
        return sora::V4L2VideoCapturer::Create(std::move(v4l2_config));
#endif
#else
        return DeviceVideoCapturer::Create(size.width, size.height,
                                           args.framerate, args.video_device);
#endif
      })();

  if (!capturer && !args.no_video_device) {
    std::cerr << "failed to create capturer" << std::endl;
    return 1;
  }

  RTCManagerConfig rtcm_config;
  rtcm_config.insecure = args.insecure;

  rtcm_config.no_video_device = args.no_video_device;
  rtcm_config.no_audio_device = args.no_audio_device;

  rtcm_config.fixed_resolution = args.fixed_resolution;
  rtcm_config.simulcast = args.sora_simulcast;
  rtcm_config.hardware_encoder_only = args.hw_mjpeg_decoder;

  rtcm_config.disable_echo_cancellation = args.disable_echo_cancellation;
  rtcm_config.disable_auto_gain_control = args.disable_auto_gain_control;
  rtcm_config.disable_noise_suppression = args.disable_noise_suppression;
  rtcm_config.disable_highpass_filter = args.disable_highpass_filter;

  rtcm_config.vp8_encoder = args.vp8_encoder;
  rtcm_config.vp8_decoder = args.vp8_decoder;
  rtcm_config.vp9_encoder = args.vp9_encoder;
  rtcm_config.vp9_decoder = args.vp9_decoder;
  rtcm_config.av1_encoder = args.av1_encoder;
  rtcm_config.av1_decoder = args.av1_decoder;
  rtcm_config.h264_encoder = args.h264_encoder;
  rtcm_config.h264_decoder = args.h264_decoder;
  rtcm_config.h265_encoder = args.h265_encoder;
  rtcm_config.h265_decoder = args.h265_decoder;

  rtcm_config.openh264 = args.openh264;
  if (!rtcm_config.openh264.empty()) {
    // If openh264 is specified, automatically use the H264 software encoder
    rtcm_config.h264_encoder = VideoCodecInfo::Type::Software;
  }

  rtcm_config.priority = args.priority;

#if defined(USE_NVCODEC_ENCODER)
  rtcm_config.cuda_context = cuda_context;
#endif

  rtcm_config.proxy_url = args.proxy_url;
  rtcm_config.proxy_username = args.proxy_username;
  rtcm_config.proxy_password = args.proxy_password;

  // Pass the congestion control algorithm to RTCManager
  rtcm_config.congestion_controller = args.congestion_controller;

#if defined(USE_FAKE_CAPTURE_DEVICE)
  // When --fake-capture-device is specified, replacing the ADM for the receiver's playback (playout) blocks it.
  // If you need to play audio with reception, maintain the platform ADM.
  // Conditions: Sora role / Ayame direction / P2P case is considered to have reception.
  bool will_recv_audio = true;
  if (use_sora) {
    will_recv_audio = (args.sora_role != "sendonly");
  } else if (use_ayame) {
    will_recv_audio = (args.ayame_direction != "sendonly");
  } else if (use_p2p) {
    will_recv_audio = true;
  }
  if (args.fake_capture_device && !args.no_audio_device && !will_recv_audio) {
    FakeAudioCapturer::Config audio_config;
    audio_config.sample_rate = 48000;
    audio_config.channels = 1;
    audio_config.fps = args.framerate;
    rtcm_config.create_adm = [audio_config, capturer]() {
      auto fake_audio_capturer = FakeAudioCapturer::Create(audio_config);
      // Set fake_audio_capturer to coordinate with FakeVideoCapturer
      static_cast<FakeVideoCapturer*>(capturer.get())
          ->SetAudioCapturer(fake_audio_capturer);
      return fake_audio_capturer;
    };
    RTC_LOG(LS_INFO)
        << "use fake audio capturer (sendonly). remote playout disabled.";
  } else if (args.fake_capture_device && will_recv_audio) {
    RTC_LOG(LS_INFO) << "fake-capture-device is set, but keeping platform ADM "
                        "to allow playout";
  }
#endif

  std::unique_ptr<SDLRenderer> sdl_renderer = nullptr;
  // Sender overlay and input capture instance (enabled only when using SDL)
  std::unique_ptr<remote::overlay::OverlayRenderer> overlay_renderer;
  std::unique_ptr<remote::input_sender::SdlInputCapture> sdl_input_capture;
  // Input DataChannel manager (kept alive)
  std::shared_ptr<remote::data_channel::InputDataManager> input_dm;
  // Input dispatcher (distributes DataChannel messages to injector/overlay)
  std::unique_ptr<remote::input_receiver::InputDispatcher> input_dispatcher;
  // Receiver (use_sdl=true) does not perform local injection, uses empty injector
  remote::input_receiver::NullInputInjector null_injector;
#ifdef _WIN32
  // Sender (use_sdl=false) uses a real injector on Windows (SendInput + ViGEm)
  remote::platform::windows::WindowsInputInjector win_injector;
  // Only the sender needs IME / cursor monitoring (reported by the controlled side)
  std::unique_ptr<remote::platform::windows::ImeMonitorWin> ime_monitor;
  std::unique_ptr<remote::platform::windows::CursorMonitorWin> cursor_monitor;
#endif
  if (args.use_sdl) {
    sdl_renderer.reset(new SDLRenderer(args.window_width, args.window_height,
                                       args.fullscreen));

    // Instantiate the overlay and input capture skeleton
    overlay_renderer = std::make_unique<remote::overlay::OverlayRenderer>();
    sdl_input_capture =
        std::make_unique<remote::input_sender::SdlInputCapture>();

    // Set SDL window for relative mouse mode (SDL3 requires window pointer)
    sdl_input_capture->SetWindow(sdl_renderer->GetWindow());

    // Auto-switch mouse mode based on cursor visibility (FPS game support)
    overlay_renderer->SetMouseModeCallback(
        [cap = sdl_input_capture.get()](bool use_relative) {
          using remote::input_sender::MouseMode;
          cap->SetMouseMode(use_relative ? MouseMode::Relative : MouseMode::Absolute);
          RTC_LOG(LS_INFO) << "Auto-switched mouse mode to " 
                           << (use_relative ? "Relative" : "Absolute") 
                           << " based on cursor visibility";
        });

    // Register the overlay render callback
    sdl_renderer->SetOverlayRenderCallback(
        [orptr = overlay_renderer.get()](SDL_Renderer* r) {
          // Call after drawing the video frame, synthesize mouse/keyboard/gamepad/RDP toolbar
          orptr->Render(r);
        });

    // Register event hooks: first give to the overlay layer (virtual keyboard/toolbar), then distribute to the input capture if not consumed
    sdl_renderer->SetEventHook(
        [rc = sdl_renderer.get(), cap = sdl_input_capture.get(),
         orptr = overlay_renderer.get()](const SDL_Event& e) {
          if (orptr && orptr->OnEvent(e)) {
            return true;  // The overlay layer has been consumed
          }
          // Update the mapping of the current primary video rectangle and the receiver (source) frame size
          int x = 0, y = 0, w = 0, h = 0, fw = 0, fh = 0;
          if (rc->GetPrimaryVideoRect(x, y, w, h, fw, fh)) {
            remote::common::Rect sdlRect{
                static_cast<float>(x), static_cast<float>(y),
                static_cast<float>(w), static_cast<float>(h)};
            remote::common::Size recvSize{fw, fh};
            cap->UpdateMapping(sdlRect, recvSize);
          }
          cap->Pump(e);
          // Initially do not intercept, return false to let SDL continue processing
          return false;
        });
  }

  std::unique_ptr<RTCManager> rtc_manager(new RTCManager(
      std::move(rtcm_config), std::move(capturer), sdl_renderer.get()));

  // Initial selection of audio output device (GUID priority, then index)
  if (!args.no_audio_device) {
    if (!args.audio_output_device_guid.empty()) {
      if (!rtc_manager->SetPlayoutDeviceByGuid(args.audio_output_device_guid)) {
        RTC_LOG(LS_WARNING)
            << __FUNCTION__ << ": Failed to set audio output device GUID="
            << args.audio_output_device_guid;
      }
    } else if (args.audio_output_device_index >= 0) {
      if (!rtc_manager->SetPlayoutDeviceByIndex(
              args.audio_output_device_index)) {
        RTC_LOG(LS_WARNING)
            << __FUNCTION__ << ": Failed to set audio output device index="
            << args.audio_output_device_index;
      }
    }
  }

  {
    boost::asio::io_context ioc{1};
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard(ioc.get_executor());

    std::shared_ptr<RTCDataManager> data_manager = nullptr;
    if (!args.serial_device.empty()) {
      data_manager = std::shared_ptr<RTCDataManager>(
          SerialDataManager::Create(ioc, args.serial_device, args.serial_rate)
              .release());
      if (!data_manager) {
        return 1;
      }
      rtc_manager->AddDataManager(data_manager);
    }

    // Add input DataChannel manager skeleton (input-reliable / input-rt)
    input_dm = std::make_shared<remote::data_channel::InputDataManager>();
    rtc_manager->AddDataManager(input_dm);

    if (args.use_sdl) {
      // Receiver (use_sdl=true): responsible for sending control events, displaying remote cursor/IME, does not perform local injection
      if (sdl_input_capture) {
        sdl_input_capture->SetSenders(
            [mgr = input_dm](const std::vector<uint8_t>& bytes) {
              return mgr->SendReliable(bytes);
            },
            [mgr = input_dm](const std::vector<uint8_t>& bytes) {
              return mgr->SendRt(bytes);
            });
      }
      if (overlay_renderer) {
        overlay_renderer->SetSenders(
            [mgr = input_dm](const std::vector<uint8_t>& bytes) {
              return mgr->SendReliable(bytes);
            },
            [mgr = input_dm](const std::vector<uint8_t>& bytes) {
              return mgr->SendRt(bytes);
            });
        overlay_renderer->SetUiCommand([rc = sdl_renderer.get()](
                                           const std::string& cmd, bool value) {
          (void)value;
          if (cmd == "fullscreen") {
            if (rc)
              rc->SetFullScreen(!rc->IsFullScreen());
          } else if (cmd == "minimize") {
            // Minimize the window
            if (rc && rc->GetWindow()) {
              SDL_MinimizeWindow(rc->GetWindow());
            }
          } else if (cmd == "close") {
            SDL_Event q{};
            q.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&q);
          }
        });
      }
    }

#ifdef REMOTE_USE_PROTOBUF
    // When using Protobuf, the channel payload is changed to binary
    input_dm->SetBinaryBoth(true);
#endif

    // Listen for DataChannel messages and process them according to the role
    if (args.use_sdl) {
      // Receiver (use_sdl=true): only used for displaying IME/cursor; does not perform local injection
      input_dispatcher =
          std::make_unique<remote::input_receiver::InputDispatcher>(
              &null_injector, overlay_renderer.get());
      input_dm->SetOnMessage(
          [disp = input_dispatcher.get()](const uint8_t* data, size_t len,
                                          bool is_binary) {
            disp->OnMessageEither(data, len, is_binary);
          });
    } else {
      // Sender (use_sdl=false): receive control messages and inject them into the local machine; report IME/cursor
#ifdef _WIN32
      input_dispatcher =
          std::make_unique<remote::input_receiver::InputDispatcher>(
              &win_injector, nullptr);
#else
      input_dispatcher =
          std::make_unique<remote::input_receiver::InputDispatcher>(
              &null_injector, nullptr);
#endif
      input_dm->SetOnMessage(
          [disp = input_dispatcher.get()](const uint8_t* data, size_t len,
                                          bool is_binary) {
            disp->OnMessageEither(data, len, is_binary);
          });
#ifdef _WIN32
      // Only the sender starts IME/cursor reporting
      ime_monitor =
          std::make_unique<remote::platform::windows::ImeMonitorWin>();
      ime_monitor->SetSender(
          [mgr = input_dm](const std::vector<uint8_t>& bytes) {
            return mgr->SendReliable(bytes);
          });
      ime_monitor->Start();
      cursor_monitor =
          std::make_unique<remote::platform::windows::CursorMonitorWin>();
      cursor_monitor->SetSender(
          [mgr = input_dm](const std::vector<uint8_t>& bytes) {
            return mgr->SendReliable(bytes);
          });
      cursor_monitor->Start();
#endif
    }

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](const boost::system::error_code&, int) { ioc.stop(); });

    std::shared_ptr<SoraClient> sora_client;
    std::shared_ptr<AyameClient> ayame_client;
    std::shared_ptr<P2PServer> p2p_server;

    MetricsServerConfig metrics_config;
    std::shared_ptr<StatsCollector> stats_collector;

    if (use_sora) {
      SoraClientConfig config;
      config.insecure = args.insecure;
      config.signaling_urls = args.sora_signaling_urls;
      config.channel_id = args.sora_channel_id;
      config.video = args.sora_video;
      config.audio = args.sora_audio;
      config.video_codec_type = args.sora_video_codec_type;
      config.audio_codec_type = args.sora_audio_codec_type;
      config.video_bit_rate = args.sora_video_bit_rate;
      config.audio_bit_rate = args.sora_audio_bit_rate;
      config.metadata = args.sora_metadata;
      config.role = args.sora_role;
      config.spotlight = args.sora_spotlight;
      config.spotlight_number = args.sora_spotlight_number;
      config.port = args.sora_port;
      config.simulcast = args.sora_simulcast;
      config.data_channel_signaling = args.sora_data_channel_signaling;
      config.data_channel_signaling_timeout =
          args.sora_data_channel_signaling_timeout;
      config.ignore_disconnect_websocket =
          args.sora_ignore_disconnect_websocket;
      config.disconnect_wait_timeout = args.sora_disconnect_wait_timeout;
      config.client_cert = args.client_cert;
      config.client_key = args.client_key;
      config.proxy_url = args.proxy_url;
      config.proxy_username = args.proxy_username;
      config.proxy_password = args.proxy_password;

      sora_client =
          SoraClient::Create(ioc, rtc_manager.get(), std::move(config));

      // If SoraServer is not started and --auto is specified, connect immediately.
      // If SoraServer is started but --auto is not specified, do not connect until the SoraServer API is called.
      if (args.sora_port < 0 || args.sora_port >= 0 && args.sora_auto_connect) {
        sora_client->Connect();
      }

      if (args.sora_port >= 0) {
        SoraServerConfig config;
        const boost::asio::ip::tcp::endpoint endpoint{
            boost::asio::ip::make_address("127.0.0.1"),
            static_cast<unsigned short>(args.sora_port)};
        SoraServer::Create(ioc, endpoint, sora_client, rtc_manager.get(),
                           config)
            ->Run();
      }

      stats_collector = sora_client;
    }

    if (use_p2p) {
      P2PServerConfig config;
      config.no_google_stun = args.no_google_stun;
      config.doc_root = args.p2p_document_root;

      const boost::asio::ip::tcp::endpoint endpoint{
          boost::asio::ip::make_address("0.0.0.0"),
          static_cast<unsigned short>(args.p2p_port)};
      p2p_server = P2PServer::Create(ioc, endpoint, rtc_manager.get(),
                                     std::move(config));
      p2p_server->Run();

      stats_collector = p2p_server;
    }

    if (use_ayame) {
      AyameClientConfig config;
      config.insecure = args.insecure;
      config.no_google_stun = args.no_google_stun;
      config.client_cert = args.client_cert;
      config.client_key = args.client_key;
      config.signaling_url = args.ayame_signaling_url;
      config.room_id = args.ayame_room_id;
      config.client_id = args.ayame_client_id;
      config.signaling_key = args.ayame_signaling_key;
      config.direction = args.ayame_direction;
      config.video_codec_type = args.ayame_video_codec_type;
      config.audio_codec_type = args.ayame_audio_codec_type;

      ayame_client =
          AyameClient::Create(ioc, rtc_manager.get(), std::move(config));
      ayame_client->Connect();

      stats_collector = ayame_client;
    }

    if (args.metrics_port >= 0) {
      const boost::asio::ip::tcp::endpoint metrics_endpoint{
          boost::asio::ip::make_address(
              args.metrics_allow_external_ip ? "0.0.0.0" : "127.0.0.1"),
          static_cast<unsigned short>(args.metrics_port)};
      MetricsServer::Create(ioc, metrics_endpoint, rtc_manager.get(),
                            stats_collector, std::move(metrics_config))
          ->Run();
    }

    if (sdl_renderer) {
      sdl_renderer->SetDispatchFunction([&ioc](std::function<void()> f) {
        if (ioc.stopped())
          return;
        boost::asio::dispatch(ioc.get_executor(), f);
      });

      ioc.run();

      sdl_renderer->SetDispatchFunction(nullptr);
    } else {
      ioc.run();
    }
  }

  // This order is clean, but not very safe
  sdl_renderer = nullptr;

  return 0;
}
