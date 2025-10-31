#include "util.h"

#include <regex>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <iostream>

// CLI11
#include <CLI/CLI.hpp>

// Boost
#include <boost/algorithm/string.hpp>
#include <boost/beast/version.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/json.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/preprocessor/stringize.hpp>

// WebRTC
#include <rtc_base/crypto_random.h>

#include "momo_version.h"

// Maximum frame rate constant
constexpr int MAX_FRAMERATE = 120;

namespace {

namespace fs = boost::filesystem;

enum class ConfigOptionType {
  Flag,
  Value,
  MultiValue,
  BoolValue,
  OptionalBool,
  LibcameraControl,
  Serial,
  Json,
};

struct ConfigOptionSpec {
  const char* section;
  const char* key;
  const char* option;
  ConfigOptionType type;
};

std::string TrimAndStripQuotes(const std::string& input) {
  auto result = boost::algorithm::trim_copy(input);
  if (result.size() >= 2) {
    auto first = result.front();
    auto last = result.back();
    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
      result = result.substr(1, result.size() - 2);
    }
  }
  return result;
}

bool ParseBool(const std::string& value, bool* parsed) {
  auto lowered = boost::algorithm::to_lower_copy(boost::algorithm::trim_copy(value));
  if (lowered.empty()) {
    return false;
  }

  if (lowered == "true" || lowered == "1" || lowered == "yes" ||
      lowered == "on") {
    *parsed = true;
    return true;
  }
  if (lowered == "false" || lowered == "0" || lowered == "no" ||
      lowered == "off") {
    *parsed = false;
    return true;
  }
  return false;
}

std::vector<std::string> ParseValueList(const std::string& value) {
  std::string normalized = value;
  for (auto& ch : normalized) {
    if (ch == ',' || ch == ';' || ch == '\n' || ch == '\r' || ch == '\t') {
      ch = ' ';
    }
  }

  std::istringstream iss(normalized);
  std::string token;
  std::vector<std::string> results;
  while (iss >> token) {
    results.emplace_back(TrimAndStripQuotes(token));
  }
  return results;
}

void AppendLibcameraControlArgs(const std::string& value,
                                const ConfigOptionSpec& spec,
                                std::vector<std::string>& args) {
  auto entries = ParseValueList(value);
  for (const auto& entry : entries) {
    if (entry.empty()) {
      continue;
    }

    std::string key;
    std::string val;
    auto delimiter = entry.find_first_of("=:");
    if (delimiter != std::string::npos) {
      key = TrimAndStripQuotes(entry.substr(0, delimiter));
      val = TrimAndStripQuotes(entry.substr(delimiter + 1));
    } else {
      std::istringstream pair_stream(entry);
      pair_stream >> key >> val;
    }

    if (key.empty() || val.empty()) {
      throw std::runtime_error("Invalid libcamera control entry: '" + entry +
                               "'");
    }

    args.emplace_back(spec.option);
    args.emplace_back(key);
    args.emplace_back(val);
  }
}

const std::vector<ConfigOptionSpec>& GetConfigOptionSpecs() {
  static const std::vector<ConfigOptionSpec> specs = [] {
    std::vector<ConfigOptionSpec> items = {
        {"general", "no_google_stun", "--no-google-stun",
         ConfigOptionType::Flag},
        {"general", "no_video_device", "--no-video-input-device",
         ConfigOptionType::Flag},
        {"general", "no_audio_device", "--no-audio-device",
         ConfigOptionType::Flag},
        {"general", "list_devices", "--list-devices",
         ConfigOptionType::Flag},
        {"general", "force_i420", "--force-i420", ConfigOptionType::Flag},
        {"general", "force_yuy2", "--force-yuy2", ConfigOptionType::Flag},
        {"general", "force_nv12", "--force-nv12", ConfigOptionType::Flag},
        {"general", "hw_mjpeg_decoder", "--hw-mjpeg-decoder",
         ConfigOptionType::BoolValue},
        {"general", "use_libcamera", "--use-libcamera",
         ConfigOptionType::Flag},
        {"general", "use_libcamera_native", "--use-libcamera-native",
         ConfigOptionType::Flag},
        {"general", "libcamera_control", "--libcamera-control",
         ConfigOptionType::LibcameraControl},
        {"general", "video_device", "--video-input-device",
         ConfigOptionType::Value},
        {"general", "resolution", "--resolution", ConfigOptionType::Value},
        {"general", "framerate", "--framerate", ConfigOptionType::Value},
        {"general", "fixed_resolution", "--fixed-resolution",
         ConfigOptionType::Flag},
        {"general", "priority", "--priority", ConfigOptionType::Value},
        {"general", "use_sdl", "--use-sdl", ConfigOptionType::Flag},
        {"general", "window_width", "--window-width",
         ConfigOptionType::Value},
        {"general", "window_height", "--window-height",
         ConfigOptionType::Value},
        {"general", "fullscreen", "--fullscreen", ConfigOptionType::Flag},
        {"general", "insecure", "--insecure", ConfigOptionType::Flag},
        {"general", "low_latency", "--low-latency", ConfigOptionType::Flag},
        {"general", "log_level", "--log-level", ConfigOptionType::Value},
        {"general", "screen_capture", "--screen-capture",
         ConfigOptionType::Flag},
        {"general", "screen_capture_cursor", "--screen-capture-cursor",
         ConfigOptionType::Flag},
        {"general", "disable_echo_cancellation",
         "--disable-echo-cancellation", ConfigOptionType::Flag},
        {"general", "disable_auto_gain_control",
         "--disable-auto-gain-control", ConfigOptionType::Flag},
        {"general", "disable_noise_suppression",
         "--disable-noise-suppression", ConfigOptionType::Flag},
        {"general", "disable_highpass_filter",
         "--disable-highpass-filter", ConfigOptionType::Flag},
        {"general", "audio_output_device_index",
         "--audio-output-device-index", ConfigOptionType::Value},
        {"general", "audio_output_device_guid",
         "--audio-output-device-guid", ConfigOptionType::Value},
        {"general", "video_codec_engines", "--video-codec-engines",
         ConfigOptionType::Flag},
        {"general", "vp8_encoder", "--vp8-encoder", ConfigOptionType::Value},
        {"general", "vp8_decoder", "--vp8-decoder", ConfigOptionType::Value},
        {"general", "vp9_encoder", "--vp9-encoder", ConfigOptionType::Value},
        {"general", "vp9_decoder", "--vp9-decoder", ConfigOptionType::Value},
        {"general", "av1_encoder", "--av1-encoder", ConfigOptionType::Value},
        {"general", "av1_decoder", "--av1-decoder", ConfigOptionType::Value},
        {"general", "h264_encoder", "--h264-encoder",
         ConfigOptionType::Value},
        {"general", "h264_decoder", "--h264-decoder",
         ConfigOptionType::Value},
        {"general", "h265_encoder", "--h265-encoder",
         ConfigOptionType::Value},
        {"general", "h265_decoder", "--h265-decoder",
         ConfigOptionType::Value},
        {"general", "openh264", "--openh264", ConfigOptionType::Value},
        {"general", "serial", "--serial", ConfigOptionType::Serial},
        {"general", "metrics_port", "--metrics-port",
         ConfigOptionType::Value},
        {"general", "metrics_allow_external_ip",
         "--metrics-allow-external-ip", ConfigOptionType::Flag},
        {"general", "client_cert", "--client-cert", ConfigOptionType::Value},
        {"general", "client_key", "--client-key", ConfigOptionType::Value},
        {"general", "proxy_url", "--proxy-url", ConfigOptionType::Value},
        {"general", "proxy_username", "--proxy-username",
         ConfigOptionType::Value},
        {"general", "proxy_password", "--proxy-password",
         ConfigOptionType::Value},
        {"general", "congestion_controller", "--cc",
         ConfigOptionType::Value},
        {"p2p", "document_root", "--document-root", ConfigOptionType::Value},
        {"p2p", "port", "--port", ConfigOptionType::Value},
        {"ayame", "signaling_url", "--signaling-url",
         ConfigOptionType::Value},
        {"ayame", "room_id", "--room-id", ConfigOptionType::Value},
        {"ayame", "client_id", "--client-id", ConfigOptionType::Value},
        {"ayame", "signaling_key", "--signaling-key",
         ConfigOptionType::Value},
        {"ayame", "direction", "--direction", ConfigOptionType::Value},
        {"ayame", "video_codec_type", "--video-codec-type",
         ConfigOptionType::Value},
        {"ayame", "audio_codec_type", "--audio-codec-type",
         ConfigOptionType::Value},
        {"sora", "signaling_urls", "--signaling-urls",
         ConfigOptionType::MultiValue},
        {"sora", "channel_id", "--channel-id", ConfigOptionType::Value},
        {"sora", "auto", "--auto", ConfigOptionType::Flag},
        {"sora", "video", "--video", ConfigOptionType::BoolValue},
        {"sora", "audio", "--audio", ConfigOptionType::BoolValue},
        {"sora", "video_codec_type", "--video-codec-type",
         ConfigOptionType::Value},
        {"sora", "audio_codec_type", "--audio-codec-type",
         ConfigOptionType::Value},
        {"sora", "video_bit_rate", "--video-bit-rate",
         ConfigOptionType::Value},
        {"sora", "audio_bit_rate", "--audio-bit-rate",
         ConfigOptionType::Value},
        {"sora", "role", "--role", ConfigOptionType::Value},
        {"sora", "spotlight", "--spotlight", ConfigOptionType::BoolValue},
        {"sora", "spotlight_number", "--spotlight-number",
         ConfigOptionType::Value},
        {"sora", "port", "--port", ConfigOptionType::Value},
        {"sora", "simulcast", "--simulcast", ConfigOptionType::BoolValue},
        {"sora", "data_channel_signaling", "--data-channel-signaling",
         ConfigOptionType::OptionalBool},
        {"sora", "data_channel_signaling_timeout",
         "--data-channel-signaling-timeout", ConfigOptionType::Value},
        {"sora", "ignore_disconnect_websocket",
         "--ignore-disconnect-websocket", ConfigOptionType::OptionalBool},
        {"sora", "disconnect_wait_timeout",
         "--disconnect-wait-timeout", ConfigOptionType::Value},
        {"sora", "metadata", "--metadata", ConfigOptionType::Json},
    };

#if defined(USE_FAKE_CAPTURE_DEVICE)
    items.emplace_back(ConfigOptionSpec{"general", "fake_capture_device",
                                        "--fake-capture-device",
                                        ConfigOptionType::Flag});
#endif

    return items;
  }();

  return specs;
}

std::string BuildPath(const ConfigOptionSpec& spec) {
  if (!spec.section || std::string(spec.section).empty()) {
    return spec.key;
  }
  return std::string(spec.section) + "." + spec.key;
}

void AppendOptionFromConfig(const boost::property_tree::ptree& pt,
                            const ConfigOptionSpec& spec,
                            std::vector<std::string>& args) {
  auto path = BuildPath(spec);
  auto value_optional = pt.get_optional<std::string>(path);
  if (!value_optional) {
    return;
  }
  auto raw_value = TrimAndStripQuotes(*value_optional);
  if (raw_value.empty()) {
    return;
  }

  switch (spec.type) {
    case ConfigOptionType::Flag: {
      bool parsed = false;
      if (!ParseBool(raw_value, &parsed)) {
        throw std::runtime_error("Invalid boolean value for '" + path +
                                 "': " + raw_value);
      }
      if (parsed) {
        args.emplace_back(spec.option);
      }
      break;
    }
    case ConfigOptionType::Value: {
      args.emplace_back(spec.option);
      args.emplace_back(raw_value);
      break;
    }
    case ConfigOptionType::BoolValue: {
      bool parsed = false;
      if (!ParseBool(raw_value, &parsed)) {
        throw std::runtime_error("Invalid boolean value for '" + path +
                                 "': " + raw_value);
      }
      args.emplace_back(spec.option);
      args.emplace_back(parsed ? "true" : "false");
      break;
    }
    case ConfigOptionType::OptionalBool: {
      auto lowered = boost::algorithm::to_lower_copy(raw_value);
      if (lowered != "true" && lowered != "false" && lowered != "none") {
        throw std::runtime_error("Invalid optional boolean value for '" + path +
                                 "': " + raw_value);
      }
      args.emplace_back(spec.option);
      args.emplace_back(lowered);
      break;
    }
    case ConfigOptionType::MultiValue: {
      auto values = ParseValueList(raw_value);
      if (values.empty()) {
        throw std::runtime_error("Empty list for multi-value option '" + path +
                                 "'");
      }
      args.emplace_back(spec.option);
      for (auto& v : values) {
        args.emplace_back(v);
      }
      break;
    }
    case ConfigOptionType::LibcameraControl: {
      AppendLibcameraControlArgs(raw_value, spec, args);
      break;
    }
    case ConfigOptionType::Serial: {
      args.emplace_back(spec.option);
      args.emplace_back(raw_value);
      break;
    }
    case ConfigOptionType::Json: {
      args.emplace_back(spec.option);
      args.emplace_back(raw_value);
      break;
    }
  }
}

void AppendSection(const boost::property_tree::ptree& pt,
                   const std::string& section,
                   std::vector<std::string>& args) {
  const auto& specs = GetConfigOptionSpecs();
  for (const auto& spec : specs) {
    if (spec.section == nullptr) {
      continue;
    }
    if (section == spec.section) {
      AppendOptionFromConfig(pt, spec, args);
    }
  }
}

std::vector<std::string> BuildArgsFromConfig(const char* program_name) {
  fs::path config_path = fs::current_path() / "config" / "config.ini";
  if (!fs::exists(config_path)) {
    throw std::runtime_error("Configuration file not found: " +
                             config_path.string());
  }

  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(config_path.string(), pt);

  auto mode_optional = pt.get_optional<std::string>("general.mode");
  if (!mode_optional) {
    throw std::runtime_error("Missing 'mode' in [general] section of " +
                             config_path.string());
  }

  auto mode = boost::algorithm::to_lower_copy(TrimAndStripQuotes(*mode_optional));
  if (mode != "p2p" && mode != "sora" && mode != "ayame") {
    throw std::runtime_error("Unsupported mode '" + mode +
                             "' in configuration. Supported modes: p2p, sora, ayame.");
  }

  std::vector<std::string> args;
  args.emplace_back(program_name);

  AppendSection(pt, "general", args);

  args.emplace_back(mode);
  AppendSection(pt, mode, args);

  return args;
}

}  // namespace

static void add_optional_bool(CLI::App* app,
                              const std::string& option_name,
                              boost::optional<bool>& v,
                              const std::string& help_text) {
  // Acceptable values: true, false, none
  auto bool_map = std::vector<std::pair<std::string, bool>>(
      {std::make_pair(std::string("false"), false),
       std::make_pair(std::string("true"), true)});

  app->add_option(option_name, v, help_text)
      ->transform(CLI::CheckedTransformer(bool_map, CLI::ignore_case))
      ->type_name("TEXT")
      ->check(CLI::IsMember({"true", "false", "none"}));
}
// Audio flags
void Util::ParseArgs(int argc,
                     char* argv[],
                     bool& use_p2p,
                     bool& use_ayame,
                     // Video encoder
                     bool& use_sora,
                     int& log_level,
                     MomoArgs& args) {
  CLI::App app("Momo - WebRTC Native Client");
  app.set_help_all_flag("--help-all", "Show help for all options (long)");

  bool version = false;
  bool video_codecs = false;
  // Proxy server settings

  auto is_valid_resolution = CLI::Validator(
      [](std::string input) -> std::string {
        if (input == "QVGA" || input == "VGA" || input == "HD" ||
            input == "FHD" || input == "4K") {
          return std::string();
        }
        // Check format WIDTHxHEIGHT
        std::regex re("^[1-9][0-9]*x[1-9][0-9]*$");
        if (std::regex_match(input, re)) {
          return std::string();
        }
        return "Must be one of QVGA, VGA, HD, FHD, 4K, or [WIDTH]x[HEIGHT].";
      },
      "resolution");

  auto is_valid_screen_capture = CLI::Validator(
      [](std::string input) -> std::string {
#if defined(USE_SCREEN_CAPTURER)
        return std::string();
#else
        return "Not available because your device does not have this feature.";
#endif
      },
      "");

  auto bool_map = std::vector<std::pair<std::string, bool>>(
      {{"false", false}, {"true", true}});

  app.add_flag("--no-google-stun", args.no_google_stun,
               "Do not use google stun");
  app.add_flag("--no-video-input-device", args.no_video_device,
               "Do not use video input device");
  app.add_flag("--no-audio-device", args.no_audio_device,
               "Do not use audio device");
  app.add_flag("--list-devices", args.list_devices,
               "List available video devices and exit");
#if defined(USE_FAKE_CAPTURE_DEVICE)
  app.add_flag("--fake-capture-device", args.fake_capture_device,
               "Use fake video capture device instead of real camera");
#endif
  app.add_flag("--force-i420", args.force_i420,
               "Force I420 format for video capture (fails if not available)");
  app.add_flag("--force-yuy2", args.force_yuy2,
               "Force YUY2 format for video capture (fails if not available)")
      ->excludes("--force-i420");  // force-* cannot be specified at the same time
  app.add_flag("--force-nv12", args.force_nv12,
               "Force NV12 format for video capture (fails if not available)")
      ->excludes("--force-i420")
      ->excludes("--force-yuy2");  // force-* cannot be specified at the same time
  app.add_option(
         "--hw-mjpeg-decoder", args.hw_mjpeg_decoder,
         "Perform MJPEG deoode and video resize by hardware acceleration "
         "(only on supported devices)")
      ->transform(CLI::CheckedTransformer(bool_map, CLI::ignore_case));
  app.add_flag("--use-libcamera", args.use_libcamera,
               "Use libcamera for video capture (only on supported devices)");
  app.add_flag("--use-libcamera-native", args.use_libcamera_native,
               "Use native buffer for H.264 encoding");
  app.add_option("--libcamera-control", args.libcamera_controls,
                 "Set libcamera control (format: key value)")
      ->allow_extra_args();

#if defined(__APPLE__) || defined(_WIN32)
  app.add_option("--video-input-device", args.video_device,
                 "Use the video device specified by an index or a name "
                 "(use the first one if not specified)");
#elif defined(__linux__)
  app.add_option("--video-input-device", args.video_device,
                 "Use the video input device specified by a name "
                 "(some device will be used if not specified)");
#endif
  app.add_option("--resolution", args.resolution,
                 "Video resolution (one of QVGA, VGA, HD, FHD, 4K, or "
                 "[WIDTH]x[HEIGHT])")
      ->check(is_valid_resolution);
  app.add_option("--framerate", args.framerate, "Video framerate")
      ->check(CLI::Range(1, MAX_FRAMERATE));
  app.add_flag("--fixed-resolution", args.fixed_resolution,
               "Maintain video resolution in degradation");
  app.add_option("--priority", args.priority,
                 "Specifies the quality that is maintained against video "
                 "degradation")
      ->check(CLI::IsMember({"BALANCE", "FRAMERATE", "RESOLUTION"}));
  app.add_flag("--use-sdl", args.use_sdl,
               "Show video using SDL (if SDL is available)");
  app.add_option("--window-width", args.window_width,
                 "Window width for videos (if SDL is available)")
      ->check(CLI::Range(180, 16384));
  app.add_option("--window-height", args.window_height,
                 "Window height for videos (if SDL is available)")
      ->check(CLI::Range(180, 16384));
  app.add_flag("--fullscreen", args.fullscreen,
               "Use fullscreen window for videos (if SDL is available)");
  app.add_flag("--version", version, "Show version information");
  app.add_flag("--insecure", args.insecure,
               "Allow insecure server connections when using SSL");
  app.add_flag("--low-latency", args.low_latency,
               "Enable low-latency rendering and pipeline tweaks (SDL vsync "
               "off, minimal render delay)");
  auto log_level_map = std::vector<std::pair<std::string, int>>(
      {{"verbose", 0}, {"info", 1}, {"warning", 2}, {"error", 3}, {"none", 4}});
  app.add_option("--log-level", log_level, "Log severity level threshold")
      ->transform(CLI::CheckedTransformer(log_level_map, CLI::ignore_case));

  app.add_flag("--screen-capture", args.screen_capture, "Capture screen")
      ->check(is_valid_screen_capture);
  app.add_flag("--screen-capture-cursor", args.screen_capture_cursor,
               "Include mouse cursor in screen capture (default: off)")
      ->check(is_valid_screen_capture);

  // Audio flags
  app.add_flag("--disable-echo-cancellation", args.disable_echo_cancellation,
               "Disable echo cancellation for audio");
  app.add_flag("--disable-auto-gain-control", args.disable_auto_gain_control,
               "Disable auto gain control for audio");
  app.add_flag("--disable-noise-suppression", args.disable_noise_suppression,
               "Disable noise suppression for audio");
  app.add_flag("--disable-highpass-filter", args.disable_highpass_filter,
               "Disable highpass filter for audio");

  // Select the audio output device
  app.add_option(
         "--audio-output-device-index", args.audio_output_device_index,
         "Select audio output device by index (0-based, -1 for default)")
      ->check(CLI::Range(-1, 128));
  app.add_option(
      "--audio-output-device-guid", args.audio_output_device_guid,
      "Select audio output device by GUID/ID (overrides index if set)");

  // Video encoder
  app.add_flag("--video-codec-engines", video_codecs,
               "List available video encoders/decoders");
  {
    auto info = VideoCodecInfo::Get();
    // Shorten for readability
    auto f = [](auto x) {
      return CLI::CheckedTransformer(VideoCodecInfo::GetValidMappingInfo(x),
                                     CLI::ignore_case);
    };
    app.add_option("--vp8-encoder", args.vp8_encoder, "VP8 Encoder")
        ->transform(f(info.vp8_encoders));
    app.add_option("--vp8-decoder", args.vp8_decoder, "VP8 Decoder")
        ->transform(f(info.vp8_decoders));
    app.add_option("--vp9-encoder", args.vp9_encoder, "VP9 Encoder")
        ->transform(f(info.vp9_encoders));
    app.add_option("--vp9-decoder", args.vp9_decoder, "VP9 Decoder")
        ->transform(f(info.vp9_decoders));
    app.add_option("--av1-encoder", args.av1_encoder, "AV1 Encoder")
        ->transform(f(info.av1_encoders));
    app.add_option("--av1-decoder", args.av1_decoder, "AV1 Decoder")
        ->transform(f(info.av1_decoders));
    app.add_option("--h264-encoder", args.h264_encoder, "H.264 Encoder")
        ->transform(f(info.h264_encoders));
    app.add_option("--h264-decoder", args.h264_decoder, "H.264 Decoder")
        ->transform(f(info.h264_decoders));
    app.add_option("--h265-encoder", args.h265_encoder, "H.265 Encoder")
        ->transform(f(info.h265_encoders));
    app.add_option("--h265-decoder", args.h265_decoder, "H.265 Decoder")
        ->transform(f(info.h265_decoders));
  }

  app.add_option("--openh264", args.openh264, "OpenH264 dynamic library path")
      ->check(CLI::ExistingFile);

  auto is_serial_setting_format = CLI::Validator(
      [](std::string input) -> std::string {
        try {
          auto separater_pos = input.find(',');
          std::string baudrate_str = input.substr(separater_pos + 1);
          unsigned int _ = std::stoi(baudrate_str);
          return std::string();
        } catch (std::invalid_argument& e) {
          return "Value " + input +
                 " is not serial setting format [DEVICE],[BAUDRATE]";
        } catch (std::out_of_range& e) {
          return "Value " + input +
                 " is not serial setting format [DEVICE],[BAUDRATE]";
        }
      },
      "serial setting format");
  std::string serial_setting;
  app.add_option("--serial", serial_setting,
                 "Serial port settings for datachannel passthrough "
                 "[DEVICE],[BAUDRATE]")
      ->check(is_serial_setting_format);

  app.add_option("--metrics-port", args.metrics_port,
                 "Metrics server port number (default: -1)")
      ->check(CLI::Range(-1, 65535));
  app.add_flag("--metrics-allow-external-ip", args.metrics_allow_external_ip,
               "Allow access to Metrics server from external IP");

  app.add_option("--client-cert", args.client_cert,
                 "Cert file path for client certification (PEM format)")
      ->check(CLI::ExistingFile);
  app.add_option("--client-key", args.client_key,
                 "Private key file path for client certification (PEM format)")
      ->check(CLI::ExistingFile);

  // proxy server settings
  app.add_option("--proxy-url", args.proxy_url, "Proxy URL");
  app.add_option("--proxy-username", args.proxy_username, "Proxy username");
  app.add_option("--proxy-password", args.proxy_password, "Proxy password");

  // Select the congestion control algorithm. The default is GCC.
  // If SQP is specified but not supported, warn and fall back to GCC.
  app.add_option("--cc", args.congestion_controller,
                 "Congestion controller (GCC or SQP, default: GCC)")
      ->check(CLI::IsMember({"GCC", "SQP", "gcc", "sqp"}));

  auto p2p_app = app.add_subcommand(
      "p2p", "P2P mode for momo development with simple HTTP server");
  auto ayame_app = app.add_subcommand(
      "ayame", "Mode for working with WebRTC Signaling Server Ayame");
  auto sora_app =
      app.add_subcommand("sora", "Mode for working with WebRTC SFU Sora");

  p2p_app
      ->add_option("--document-root", args.p2p_document_root,
                   "HTTP document root directory")
      ->check(CLI::ExistingDirectory);
  p2p_app->add_option("--port", args.p2p_port, "Port number (default: 8080)")
      ->check(CLI::Range(0, 65535));

  ayame_app
      ->add_option("--signaling-url", args.ayame_signaling_url, "Signaling URL")
      ->required();
  ayame_app->add_option("--room-id", args.ayame_room_id, "Room ID")->required();
  ayame_app->add_option("--client-id", args.ayame_client_id, "Client ID");
  ayame_app->add_option("--signaling-key", args.ayame_signaling_key,
                        "Signaling key");
  ayame_app
      ->add_option("--direction", args.ayame_direction,
                   "Direction (default: sendrecv)")
      ->check(CLI::IsMember({"sendrecv", "sendonly", "recvonly"}));
  ayame_app
      ->add_option("--video-codec-type", args.ayame_video_codec_type,
                   "Video codec type (VP8, VP9, AV1, H264, H265)")
      ->check(CLI::IsMember({"", "VP8", "VP9", "AV1", "H264", "H265"}));
  ayame_app
      ->add_option("--audio-codec-type", args.ayame_audio_codec_type,
                   "Audio codec type (OPUS, PCMU, PCMA)")
      ->check(CLI::IsMember({"", "OPUS", "PCMU", "PCMA"}));

  sora_app
      ->add_option("--signaling-urls", args.sora_signaling_urls,
                   "Signaling URLs")
      ->take_all()
      ->required();
  sora_app->add_option("--channel-id", args.sora_channel_id, "Channel ID")
      ->required();
  sora_app->add_flag("--auto", args.sora_auto_connect,
                     "Connect to Sora automatically");

  sora_app
      ->add_option("--video", args.sora_video,
                   "Send video to sora (default: true)")
      ->transform(CLI::CheckedTransformer(bool_map, CLI::ignore_case));
  sora_app
      ->add_option("--audio", args.sora_audio,
                   "Send audio to sora (default: true)")
      ->transform(CLI::CheckedTransformer(bool_map, CLI::ignore_case));
  sora_app
      ->add_option("--video-codec-type", args.sora_video_codec_type,
                   "Video codec for send")
      ->check(CLI::IsMember({"", "VP8", "VP9", "AV1", "H264", "H265"}));
  sora_app
      ->add_option("--audio-codec-type", args.sora_audio_codec_type,
                   "Audio codec for send")
      ->check(CLI::IsMember({"", "OPUS"}));
  sora_app
      ->add_option("--video-bit-rate", args.sora_video_bit_rate,
                   "Video bit rate")
      ->check(CLI::Range(0, 30000));
  sora_app
      ->add_option("--audio-bit-rate", args.sora_audio_bit_rate,
                   "Audio bit rate")
      ->check(CLI::Range(0, 510));
  sora_app->add_option("--role", args.sora_role, "Role (default: sendonly)")
      ->check(CLI::IsMember({"sendonly", "recvonly", "sendrecv"}));
  sora_app->add_option("--spotlight", args.sora_spotlight, "Use spotlight")
      ->transform(CLI::CheckedTransformer(bool_map, CLI::ignore_case));
  sora_app
      ->add_option("--spotlight-number", args.sora_spotlight_number,
                   "Stream count delivered in spotlight")
      ->check(CLI::Range(0, 8));
  sora_app->add_option("--port", args.sora_port, "Port number (default: -1)")
      ->check(CLI::Range(-1, 65535));
  sora_app
      ->add_option("--simulcast", args.sora_simulcast,
                   "Use simulcast (default: false)")
      ->transform(CLI::CheckedTransformer(bool_map, CLI::ignore_case));
  add_optional_bool(sora_app, "--data-channel-signaling",
                    args.sora_data_channel_signaling,
                    "Use DataChannel for Sora signaling (default: none)");
  sora_app
      ->add_option("--data-channel-signaling-timeout",
                   args.sora_data_channel_signaling_timeout,
                   "Timeout for Data Channel in seconds (default: 180)")
      ->check(CLI::PositiveNumber);
  add_optional_bool(sora_app, "--ignore-disconnect-websocket",
                    args.sora_ignore_disconnect_websocket,
                    "Ignore WebSocket disconnection if using Data Channel "
                    "(default: none)");
  sora_app
      ->add_option(
          "--disconnect-wait-timeout", args.sora_disconnect_wait_timeout,
          "Disconnecting timeout for Data Channel in seconds (default: 5)")
      ->check(CLI::PositiveNumber);

  auto is_json = CLI::Validator(
      [](std::string input) -> std::string {
        boost::system::error_code ec;
        boost::json::parse(input, ec);
        if (ec) {
          return "Value " + input + " is not JSON Value";
        }
        return std::string();
      },
      "JSON Value");
  std::string sora_metadata;
  sora_app
      ->add_option("--metadata", sora_metadata,
                   "Signaling metadata used in connect message")
      ->check(is_json);

  bool parsed_from_config = false;
  std::vector<std::string> config_storage;
  if (argc <= 1) {
    try {
      config_storage = BuildArgsFromConfig(argv[0]);
      parsed_from_config = true;
    } catch (const std::exception& e) {
      std::cerr << "Failed to load configuration: " << e.what() << std::endl;
      exit(1);
    }
  }

  try {
    if (parsed_from_config) {
      std::vector<char*> argv_ptrs;
      argv_ptrs.reserve(config_storage.size());
      for (auto& entry : config_storage) {
        argv_ptrs.push_back(const_cast<char*>(entry.c_str()));
      }
      app.parse(static_cast<int>(argv_ptrs.size()), argv_ptrs.data());
    } else {
      app.parse(argc, argv);
    }
  } catch (const CLI::ParseError& e) {
    exit(app.exit(e));
  }

  if (!serial_setting.empty()) {
    auto separater_pos = serial_setting.find(',');
    std::string baudrate_str = serial_setting.substr(separater_pos + 1);
    args.serial_device = serial_setting.substr(0, separater_pos);
    args.serial_rate = std::stoi(baudrate_str);
  }

  // Parse the metadata
  if (!sora_metadata.empty()) {
    args.sora_metadata = boost::json::parse(sora_metadata);
  }

  if (args.p2p_document_root.empty()) {
    args.p2p_document_root = boost::filesystem::current_path().string();
  }

  if (version) {
    // std::cout << MomoVersion::GetClientName() << std::endl;
    // std::cout << std::endl;
    // std::cout << "WebRTC: " << MomoVersion::GetLibwebrtcName() << std::endl;
    // std::cout << "Environment: " << MomoVersion::GetEnvironmentName()
              // << std::endl;
    // std::cout << std::endl;
#if defined(USE_JETSON_ENCODER)
    // std::cout << "- USE_JETSON_ENCODER";
#endif
#if defined(USE_NVCODEC_ENCODER)
    // std::cout << "- USE_NVCODEC_ENCODER";
#endif
#if defined(USE_V4L2_ENCODER)
    // std::cout << "- USE_V4L2_ENCODER";
#endif
#if defined(USE_VPL_ENCODER)
    // std::cout << "- USE_VPL_ENCODER";
#endif
    exit(0);
  }

  if (video_codecs) {
    ShowVideoCodecs(VideoCodecInfo::Get());
    exit(0);
  }

  // If --list-devices is specified, skip the subcommand check
  if (args.list_devices) {
    // main.cpp is processed, so do nothing here
    return;
  }

  if (!p2p_app->parsed() && !sora_app->parsed() && !ayame_app->parsed()) {
    // std::cout << app.help() << std::endl;
    exit(1);
  }

  if (sora_app->parsed()) {
    use_sora = true;
  }

  if (p2p_app->parsed()) {
    use_p2p = true;
  }

  if (ayame_app->parsed()) {
    use_ayame = true;
  }
}

void Util::ShowVideoCodecs(VideoCodecInfo info) {
  // VP8:
  //   Encoder:
  //     - Software (default)
  //   Decoder:
  //     - Jetson (default)
  //     - Software
  //
  // VP9:
  //   Encoder:
  //     - Software (default)
  // ...
  //
  // Output like this
  auto list_codecs = [](std::vector<VideoCodecInfo::Type> types) {
    if (types.empty()) {
      // std::cout << "    *UNAVAILABLE*" << std::endl;
      return;
    }

    for (int i = 0; i < types.size(); i++) {
      auto type = types[i];
      auto p = VideoCodecInfo::TypeToString(type);
      // std::cout << "    - " << p.first << " [" << p.second << "]";
      if (i == 0) {
        // std::cout << " (default)";
      }
      // std::cout << std::endl;
    }
  };
  // std::cout << "VP8:" << std::endl;
  // std::cout << "  Encoder:" << std::endl;
  list_codecs(info.vp8_encoders);
  // std::cout << "  Decoder:" << std::endl;
  list_codecs(info.vp8_decoders);
  // std::cout << "" << std::endl;
  // std::cout << "VP9:" << std::endl;
  // std::cout << "  Encoder:" << std::endl;
  list_codecs(info.vp9_encoders);
  // std::cout << "  Decoder:" << std::endl;
  list_codecs(info.vp9_decoders);
  // std::cout << "" << std::endl;
  // std::cout << "AV1:" << std::endl;
  // std::cout << "  Encoder:" << std::endl;
  list_codecs(info.av1_encoders);
  // std::cout << "  Decoder:" << std::endl;
  list_codecs(info.av1_decoders);
  // std::cout << "" << std::endl;
  // std::cout << "H264:" << std::endl;
  // std::cout << "  Encoder:" << std::endl;
  list_codecs(info.h264_encoders);
  // std::cout << "  Decoder:" << std::endl;
  list_codecs(info.h264_decoders);
  // std::cout << "" << std::endl;
  // std::cout << "H265:" << std::endl;
  // std::cout << "  Encoder:" << std::endl;
  list_codecs(info.h265_encoders);
  // std::cout << "  Decoder:" << std::endl;
  list_codecs(info.h265_decoders);
}

std::string Util::GenerateRandomChars() {
  return GenerateRandomChars(32);
}

std::string Util::GenerateRandomChars(size_t length) {
  std::string result;
  webrtc::CreateRandomString(length, &result);
  return result;
}

std::string Util::GenerateRandomNumericChars(size_t length) {
  auto random_numerics = []() -> char {
    const char charset[] = "0123456789";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };
  std::string result(length, 0);
  std::generate_n(result.begin(), length, random_numerics);
  return result;
}

std::string Util::IceConnectionStateToString(
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  switch (state) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
      return "new";
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      return "checking";
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
      return "connected";
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
      return "completed";
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
      return "failed";
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
      return "disconnected";
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
      return "closed";
    case webrtc::PeerConnectionInterface::kIceConnectionMax:
      return "max";
  }
  return "unknown";
}

namespace http = boost::beast::http;
using string_view = boost::beast::string_view;

string_view Util::MimeType(string_view path) {
  using boost::beast::iequals;
  auto const ext = [&path] {
    auto const pos = path.rfind(".");
    if (pos == string_view::npos)
      return string_view{};
    return path.substr(pos);
  }();

  if (iequals(ext, ".htm"))
    return "text/html";
  if (iequals(ext, ".html"))
    return "text/html";
  if (iequals(ext, ".php"))
    return "text/html";
  if (iequals(ext, ".css"))
    return "text/css";
  if (iequals(ext, ".txt"))
    return "text/plain";
  if (iequals(ext, ".js"))
    return "application/javascript";
  if (iequals(ext, ".json"))
    return "application/json";
  if (iequals(ext, ".xml"))
    return "application/xml";
  if (iequals(ext, ".swf"))
    return "application/x-shockwave-flash";
  if (iequals(ext, ".flv"))
    return "video/x-flv";
  if (iequals(ext, ".png"))
    return "image/png";
  if (iequals(ext, ".jpe"))
    return "image/jpeg";
  if (iequals(ext, ".jpeg"))
    return "image/jpeg";
  if (iequals(ext, ".jpg"))
    return "image/jpeg";
  if (iequals(ext, ".gif"))
    return "image/gif";
  if (iequals(ext, ".bmp"))
    return "image/bmp";
  if (iequals(ext, ".ico"))
    return "image/vnd.microsoft.icon";
  if (iequals(ext, ".tiff"))
    return "image/tiff";
  if (iequals(ext, ".tif"))
    return "image/tiff";
  if (iequals(ext, ".svg"))
    return "image/svg+xml";
  if (iequals(ext, ".svgz"))
    return "image/svg+xml";
  return "application/text";
}

http::response<http::string_body> Util::BadRequest(
    const http::request<http::string_body>& req,
    string_view why) {
  http::response<http::string_body> res{http::status::bad_request,
                                        req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "text/html");
  res.keep_alive(req.keep_alive());
  res.body() = why;
  res.prepare_payload();
  return res;
}

http::response<http::string_body> Util::NotFound(
    const http::request<http::string_body>& req,
    string_view target) {
  http::response<http::string_body> res{http::status::not_found, req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "text/html");
  res.keep_alive(req.keep_alive());
  res.body() = "The resource '" + std::string(target) + "' was not found.";
  res.prepare_payload();
  return res;
}

http::response<http::string_body> Util::ServerError(
    const http::request<http::string_body>& req,
    string_view what) {
  http::response<http::string_body> res{http::status::internal_server_error,
                                        req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "text/html");
  res.keep_alive(req.keep_alive());
  res.body() = "An error occurred: '" + std::string(what) + "'";
  res.prepare_payload();
  return res;
}
