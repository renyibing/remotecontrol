#include "ayame_client.h"

// boost
#include <boost/beast/websocket/stream.hpp>
#include <boost/json.hpp>

// WebRTC
#include <api/peer_connection_interface.h>
#include <api/rtp_transceiver_interface.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "momo_version.h"
#include "ssl_verifier.h"
#include "url_parts.h"
#include "util.h"

namespace {
// Video auxiliary codecs
const std::vector<std::string> kVideoAuxiliaryCodecs = {"rtx", "red", "ulpfec",
                                                        "flexfec-03"};

// Audio auxiliary codecs
const std::vector<std::string> kAudioAuxiliaryCodecs = {"telephone-event",
                                                        "cn"};

constexpr int kInitialWatchdogTimeoutSeconds = 30;
constexpr int kConnectedWatchdogTimeoutSeconds = 60;
constexpr int kReconnectIntervalStepSeconds = 10;
constexpr int kReconnectIntervalMaxSeconds = 30;

// Check if the codec is an auxiliary codec
bool IsAuxiliaryCodec(const std::string& codec_name,
                      webrtc::MediaType media_type) {
  const auto& auxiliary_codecs = media_type == webrtc::MediaType::VIDEO
                                     ? kVideoAuxiliaryCodecs
                                     : kAudioAuxiliaryCodecs;
  return std::find_if(auxiliary_codecs.begin(), auxiliary_codecs.end(),
                      [&codec_name](const std::string& codec) {
                        return absl::EqualsIgnoreCase(codec, codec_name);
                      }) != auxiliary_codecs.end();
}

bool ParseURL(const std::string& url, URLParts& parts, bool& ssl) {
  if (!URLParts::Parse(url, parts)) {
    return false;
  }

  if (parts.scheme == "wss") {
    ssl = true;
    return true;
  } else if (parts.scheme == "ws") {
    ssl = false;
    return true;
  } else {
    return false;
  }
}

webrtc::PeerConnectionInterface::IceServers CreateIceServersFromConfig(
    boost::json::value json_message,
    bool no_google_stun) {
  webrtc::PeerConnectionInterface::IceServers ice_servers;

  // Set the returned iceServers
  if (json_message.as_object().contains("iceServers")) {
    for (const auto& j_ice_server_value :
         json_message.at("iceServers").as_array()) {
      const auto& j_ice_server = j_ice_server_value.as_object();
      webrtc::PeerConnectionInterface::IceServer ice_server;
      if (j_ice_server.contains("username")) {
        ice_server.username = j_ice_server.at("username").as_string().c_str();
      }
      if (j_ice_server.contains("credential")) {
        ice_server.password = j_ice_server.at("credential").as_string().c_str();
      }
      for (const auto& j_url_value : j_ice_server.at("urls").as_array()) {
        ice_server.urls.push_back(j_url_value.as_string().c_str());
      }
      ice_servers.push_back(ice_server);
    }
  }

  // Append default Coturn/STUN servers (temporary until WS config is implemented)
  auto append_default_servers = [&]() {
    webrtc::PeerConnectionInterface::IceServer stun_server;
    stun_server.urls.push_back("stun:xxx.xxx.xxx.xxx:xxxx");
    ice_servers.push_back(stun_server);

    webrtc::PeerConnectionInterface::IceServer turn_server_udp;
    turn_server_udp.urls.push_back("turn:xxx.xxx.xxx.xxx:xxxx?transport=udp");
    turn_server_udp.username = "x";
    turn_server_udp.password = "x";
    ice_servers.push_back(turn_server_udp);

    webrtc::PeerConnectionInterface::IceServer turn_server_tcp;
    turn_server_tcp.urls.push_back("turn:xxx.xxx.xxx.xxx:xxxx?transport=tcp");
    turn_server_tcp.username = "x";
    turn_server_tcp.password = "x";
    ice_servers.push_back(turn_server_tcp);
  };
  append_default_servers();

  if (ice_servers.empty() && !no_google_stun) {
    // If iceServers are not returned, use the Google STUN server
    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.urls.push_back("stun:stun.l.google.com:19302");
    ice_servers.push_back(ice_server);
  }

  return ice_servers;
}

void SetCodecPreferences(
    std::shared_ptr<RTCConnection> connection,
    const std::string& video_codec_type,
    const std::string& audio_codec_type,
    webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory) {
  if (video_codec_type.empty() && audio_codec_type.empty()) {
    return;
  }

  auto pc = connection->GetConnection();
  if (pc == nullptr) {
    RTC_LOG(LS_ERROR) << "PeerConnection is null";
    return;
  }

  auto transceivers = pc->GetTransceivers();

  // If the transceiver does not exist, output an error and do nothing
  if (transceivers.empty()) {
    RTC_LOG(LS_ERROR)
        << "No transceivers found when trying to set codec preferences";
    return;
  }

  for (auto transceiver : transceivers) {
    const auto media_type = transceiver->media_type();
    const bool is_video = media_type == webrtc::MediaType::VIDEO;
    const bool is_audio = media_type == webrtc::MediaType::AUDIO;

    // If the transceiver is not video or audio, skip
    if (!is_video && !is_audio) {
      continue;
    }

    // If the specified codec is empty, skip
    const bool codec_not_specified = (is_video && video_codec_type.empty()) ||
                                     (is_audio && audio_codec_type.empty());
    if (codec_not_specified) {
      continue;
    }

    // Determine the target codec based on media type
    const std::string target_codec = is_video ? video_codec_type : audio_codec_type;

    // Get the codec capabilities from the factory
    auto sender_capabilities = factory->GetRtpSenderCapabilities(media_type);
    auto receiver_capabilities = factory->GetRtpReceiverCapabilities(media_type);

    // Find the codecs supported by both the sender and receiver
    std::vector<webrtc::RtpCodecCapability> common_codecs;
    for (const auto& sender_codec : sender_capabilities.codecs) {
      for (const auto& receiver_codec : receiver_capabilities.codecs) {
        // If the MIME type matches, consider it a common codec
        if (sender_codec.mime_type() == receiver_codec.mime_type()) {
          common_codecs.push_back(sender_codec);
          break;
        }
      }
    }

    // If the common codecs are empty, skip
    if (common_codecs.empty()) {
      RTC_LOG(LS_WARNING)
          << "No common codec capabilities available for transceiver";
      continue;
    }

    RTC_LOG(LS_INFO) << "Found " << common_codecs.size()
                     << " common codecs for "
                     << webrtc::MediaTypeToString(media_type);

    // Filter the codecs
    std::vector<webrtc::RtpCodecCapability> primary_codecs;
    std::vector<webrtc::RtpCodecCapability> auxiliary_codecs;
    for (const auto& codec : common_codecs) {
      if (absl::EqualsIgnoreCase(codec.name, target_codec)) {
        primary_codecs.push_back(codec);
        continue;
      }
      if (IsAuxiliaryCodec(codec.name, media_type)) {
        auxiliary_codecs.push_back(codec);
      }
    }

    // If the specified codec is not found, output an error
    if (primary_codecs.empty()) {
      RTC_LOG(LS_ERROR) << "Specified codec '" << target_codec << "' for "
                        << webrtc::MediaTypeToString(transceiver->media_type())
                        << " is not available. Available codecs:";
      for (const auto& codec : common_codecs) {
        RTC_LOG(LS_ERROR) << "  - " << codec.name;
      }
      continue;
    }

    std::vector<webrtc::RtpCodecCapability> filtered_codecs;
    filtered_codecs.insert(filtered_codecs.end(), primary_codecs.begin(),
                           primary_codecs.end());
    filtered_codecs.insert(filtered_codecs.end(), auxiliary_codecs.begin(),
                           auxiliary_codecs.end());

    auto error = transceiver->SetCodecPreferences(filtered_codecs);
    if (!error.ok()) {
      RTC_LOG(LS_ERROR) << "Failed to set codec preferences: "
                        << error.message();
      continue;
    }

    RTC_LOG(LS_INFO) << "Successfully set codec preferences for "
                     << webrtc::MediaTypeToString(transceiver->media_type());
  }
}

std::shared_ptr<RTCConnection> CreateRTCConnection(
    RTCManager* manager,
    RTCMessageSender* sender,
    const webrtc::PeerConnectionInterface::IceServers& ice_servers,
    const std::optional<std::string>& direction,
    const std::string& video_codec_type,
    const std::string& audio_codec_type) {
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;

  rtc_config.servers = ice_servers;
  std::shared_ptr<RTCConnection> connection =
      manager->CreateConnection(rtc_config, sender);
  if (!connection) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": failed to create RTC connection";
    return nullptr;
  }
  manager->InitTracks(connection.get(), direction);

  // To get the codec list using PeerConnectionFactory::GetRtpSenderCapabilities(), get the PeerConnectionFactory
  auto factory = manager->GetFactory();
  if (factory == nullptr) {
    RTC_LOG(LS_ERROR) << "PeerConnectionFactory is null";
    return nullptr;
  }

  // After the Transceiver is created in InitTracks, call SetCodecPreferences
  SetCodecPreferences(connection, video_codec_type, audio_codec_type, factory);

  return connection;
}

}  // namespace

void AyameClient::GetStats(
    std::function<void(
        const webrtc::scoped_refptr<const webrtc::RTCStatsReport>&)> callback) {
  if (connection_ &&
      (rtc_state_ == webrtc::PeerConnectionInterface::IceConnectionState::
                         kIceConnectionConnected ||
       rtc_state_ == webrtc::PeerConnectionInterface::IceConnectionState::
                         kIceConnectionCompleted)) {
    connection_->GetStats(std::move(callback));
  } else {
    callback(nullptr);
  }
}

AyameClient::AyameClient(boost::asio::io_context& ioc,
                         RTCManager* manager,
                         AyameClientConfig config)
    : ioc_(ioc),
      manager_(manager),
      config_(std::move(config)),
      watchdog_(ioc, std::bind(&AyameClient::OnWatchdogExpired, this)) {
  Reset();
}

AyameClient::~AyameClient() {
  destructed_ = true;
  // OnIceConnectionStateChange is called here
  connection_ = nullptr;
}

void AyameClient::Reset() {
  connection_ = nullptr;
  is_send_offer_ = false;
  has_is_exist_user_flag_ = false;
  ice_servers_.clear();

  // Before creating a new WebSocket, explicitly destroy it and then verify the URL
  ws_.reset();

  URLParts parts;
  bool use_tls;
  if (!ParseURL(config_.signaling_url, parts, use_tls)) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": failed to prepare signaling url: "
                      << config_.signaling_url;
    throw std::runtime_error("Failed to parse signaling url: " +
                             config_.signaling_url);
  }

  if (use_tls) {
    ws_ = std::make_unique<Websocket>(Websocket::ssl_tag(), ioc_,
                                      config_.insecure, config_.client_cert,
                                      config_.client_key);
  } else {
    ws_ = std::make_unique<Websocket>(ioc_);
  }
}

void AyameClient::Connect() {
  RTC_LOG(LS_INFO) << __FUNCTION__;

  watchdog_.Enable(kInitialWatchdogTimeoutSeconds);

  ws_->Connect(config_.signaling_url,
               std::bind(&AyameClient::OnConnect, shared_from_this(),
                         std::placeholders::_1));
}

void AyameClient::ReconnectAfter() {
  // Increase the delay time according to retry_count_, avoid the upper limit and overflow
  const int64_t interval_raw =
      static_cast<int64_t>(retry_count_) * kReconnectIntervalStepSeconds;
  const int64_t clamped_interval =
      std::min<int64_t>(interval_raw, kReconnectIntervalMaxSeconds);
  const int interval = static_cast<int>(clamped_interval);

  RTC_LOG(LS_INFO) << __FUNCTION__ << " reconnect after " << interval << " sec";

  watchdog_.Enable(interval);
  retry_count_++;
}

void AyameClient::OnWatchdogExpired() {
  RTC_LOG(LS_WARNING) << __FUNCTION__;

  RTC_LOG(LS_INFO) << __FUNCTION__ << " reconnecting...:";
  Reset();
  Connect();
}

void AyameClient::OnConnect(boost::system::error_code ec) {
  if (ec) {
    ReconnectAfter();
    return MOMO_BOOST_ERROR(ec, "Handshake");
  }

  DoRead();

  DoRegister();
}

void AyameClient::DoRead() {
  ws_->Read(std::bind(&AyameClient::OnRead, shared_from_this(),
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3));
}

void AyameClient::DoRegister() {
  boost::json::value json_message = {
      {"type", "register"},
      {"clientId", Util::GenerateRandomChars()},
      {"roomId", config_.room_id},
      {"ayameClient", MomoVersion::GetClientName()},
      {"libwebrtc", MomoVersion::GetLibwebrtcName()},
      {"environment", MomoVersion::GetEnvironmentName()},
  };
  if (config_.client_id != "") {
    json_message.as_object()["clientId"] = config_.client_id;
  }
  if (config_.signaling_key != "") {
    json_message.as_object()["key"] = config_.signaling_key;
  }
  ws_->WriteText(boost::json::serialize(json_message));
}

void AyameClient::DoSendPong() {
  boost::json::value json_message = {{"type", "pong"}};
  ws_->WriteText(boost::json::serialize(json_message));
}

void AyameClient::Close() {
  ws_->Close(std::bind(&AyameClient::OnClose, shared_from_this(),
                       std::placeholders::_1));
}

// Callback when the WebSocket is closed
void AyameClient::OnClose(boost::system::error_code ec) {
  if (ec) {
    MOMO_BOOST_ERROR(ec, "Close");
  }
  // retry_count_ may have been incremented by ReconnectAfter() if it was called before.
  // To minimize the time until OnWatchdogExpired(); is fired and the reconnection is performed, set retry_count_ to 0 if it was incremented by ReconnectAfter() before.
  retry_count_ = 0;
  // If the WebSocket is properly closed, fire ReconnectAfter();
  // ReconnectAfter(); calls OnWatchdogExpired();, and the WebSocket reconnection is performed here.
  // Currently, the WebSocket is reconnected regardless of the reason it is closed.
  ReconnectAfter();
}

void AyameClient::OnRead(boost::system::error_code ec,
                         [[maybe_unused]] std::size_t bytes_transferred,
                         std::string text) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << ": " << ec;

  // If the read operation is canceled for writing, this error occurs, so it is not treated as an error
  if (ec == boost::asio::error::operation_aborted)
    return;

  // If the WebSocket returns a closed error, immediately call Close(); to exit the OnRead function
  if (ec == boost::beast::websocket::error::closed) {
    // When the WebSocket is closed by Close();, the functions OnClose(); -> ReconnectAfter(); -> OnWatchdogExpired(); are called in order, and the WebSocket reconnection is performed
    Close();
    return;
  }

  if (ec) {
    return MOMO_BOOST_ERROR(ec, "Read");
  }

  RTC_LOG(LS_INFO) << __FUNCTION__ << ": text=" << text;

  auto json_message = boost::json::parse(text);
  const std::string type = json_message.at("type").as_string().c_str();
  if (type == "accept") {
    ice_servers_ =
        CreateIceServersFromConfig(json_message, config_.no_google_stun);
    connection_ =
        CreateRTCConnection(manager_, this, ice_servers_, config_.direction,
                            config_.video_codec_type, config_.audio_codec_type);
    if (!connection_) {
      RTC_LOG(LS_ERROR) << __FUNCTION__
                        << ": peer connection setup failed at accept";
      Close();
      return;
    }
    // Check if the isExistUser flag exists
    auto is_exist_user = false;
    if (json_message.as_object().contains("isExistUser")) {
      has_is_exist_user_flag_ = true;
      is_exist_user = json_message.at("isExistUser").as_bool();
    }

    auto on_create_offer = [this](webrtc::SessionDescriptionInterface* desc) {
      std::string sdp;
      desc->ToString(&sdp);
      manager_->SetParameters();
      boost::json::value json_message = {{"type", "offer"}, {"sdp", sdp}};
      ws_->WriteText(boost::json::serialize(json_message));
    };

    // If the isExistUser flag exists and is true, generate and send the offer SDP
    if (is_exist_user) {
      RTC_LOG(LS_INFO) << __FUNCTION__ << ": exist_user";
      is_send_offer_ = true;
      connection_->CreateOffer(on_create_offer);
    } else if (!has_is_exist_user_flag_) {
      // If the flag is not present, send it anyway
      connection_->CreateOffer(on_create_offer);
    }
  } else if (type == "offer") {
    // If the isExistUser flag is not present, generate the peer connection again
    if (!has_is_exist_user_flag_) {
      connection_ = CreateRTCConnection(
          manager_, this, ice_servers_, config_.direction,
          config_.video_codec_type, config_.audio_codec_type);
    }
    if (!connection_) {
      RTC_LOG(LS_ERROR) << __FUNCTION__
                        << ": peer connection is not ready for offer";
      Close();
      return;
    }
    const std::string sdp = json_message.at("sdp").as_string().c_str();
    auto self = shared_from_this();
    connection_->SetOffer(sdp, [self]() {
      boost::asio::post(self->ioc_, [self]() {
        // Conditions for creating an Answer:
        // 1. If you have not sent an Offer from yourself (!is_send_offer_)
        // 2. If the isExistUser flag is not present (!has_is_exist_user_flag_)
        // If the isExistUser flag is present, it indicates that there is an existing user,
        // and Answer is created only when the second Offer is received
        const bool should_create_answer =
            !self->is_send_offer_ || !self->has_is_exist_user_flag_;
        if (should_create_answer) {
          self->connection_->CreateAnswer(
              [self](webrtc::SessionDescriptionInterface* desc) {
                std::string sdp;
                desc->ToString(&sdp);
                self->manager_->SetParameters();
                boost::json::value json_message = {{"type", "answer"},
                                                   {"sdp", sdp}};
                self->ws_->WriteText(boost::json::serialize(json_message));
              });
        }
        self->is_send_offer_ = false;
      });
    });
  } else if (type == "answer") {
    const std::string sdp = json_message.at("sdp").as_string().c_str();
    connection_->SetAnswer(sdp);
  } else if (type == "candidate") {
    boost::json::value ice = json_message.at("ice");
    std::string sdp_mid = ice.at("sdpMid").as_string().c_str();
    int sdp_mlineindex = ice.at("sdpMLineIndex").to_number<int>();
    std::string candidate = ice.at("candidate").as_string().c_str();
    connection_->AddIceCandidate(sdp_mid, sdp_mlineindex, candidate);
  } else if (type == "ping") {
    watchdog_.Reset();
    DoSendPong();
  } else if (type == "bye") {
    RTC_LOG(LS_INFO) << __FUNCTION__ << ": bye";
    connection_ = nullptr;
    Close();
  }
  DoRead();
}

// Callback from WebRTC
// These are called from a different thread, so handle with care
void AyameClient::OnIceConnectionStateChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " state:" << new_state;
  // If the destructor is called, shared_from_this does not work, so ignore it
  if (destructed_) {
    return;
  }
  boost::asio::post(ioc_, std::bind(&AyameClient::DoIceConnectionStateChange,
                                    shared_from_this(), new_state));
}
void AyameClient::OnIceCandidate(const std::string sdp_mid,
                                 const int sdp_mlineindex,
                                 const std::string sdp) {
  // In Ayame, the candidate SDP exchange uses the `ice` property. Note that it is not `candidate`.
  boost::json::value json_message = {
      {"type", "candidate"},
  };
  // Set the candidate information in the `ice` property and send it
  json_message.as_object()["ice"] = {{"candidate", sdp},
                                     {"sdpMLineIndex", sdp_mlineindex},
                                     {"sdpMid", sdp_mid}};
  ws_->WriteText(boost::json::serialize(json_message));
}

void AyameClient::DoIceConnectionStateChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << ": newState="
                   << Util::IceConnectionStateToString(new_state);

  switch (new_state) {
    case webrtc::PeerConnectionInterface::IceConnectionState::
        kIceConnectionConnected:
      retry_count_ = 0;
      watchdog_.Enable(kConnectedWatchdogTimeoutSeconds);
      break;
    // If the ice connection state fails, call Close(); to close the WebSocket connection
    case webrtc::PeerConnectionInterface::IceConnectionState::
        kIceConnectionFailed:
      // When the WebSocket is closed by Close();, the functions OnClose(); -> ReconnectAfter(); -> OnWatchdogExpired(); are called in order, and the WebSocket reconnection is performed
      Close();
      break;
    default:
      break;
  }
  rtc_state_ = new_state;
}
