#ifndef AYAME_CLIENT_H_
#define AYAME_CLIENT_H_

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

// Boost
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/json.hpp>

#include "metrics/stats_collector.h"
#include "rtc/rtc_manager.h"
#include "rtc/rtc_message_sender.h"
#include "url_parts.h"
#include "watchdog.h"
#include "websocket.h"

struct AyameClientConfig {
  bool insecure = false;
  bool no_google_stun = false;
  std::string client_cert;
  std::string client_key;

  std::string signaling_url;
  std::string room_id;
  std::string client_id;
  std::string signaling_key;
  // sendrecv, sendonly, recvonly
  std::string direction;
  std::string video_codec_type;
  std::string audio_codec_type;
};

// AyameClient is designed to run on a single thread.
// The io_context is driven by a single thread, and all event processing is
// performed serially on this io_context.
// The callbacks from WebRTC are transferred to the io_context using boost::asio::post,
// so access to member variables does not require synchronization.
//
// Note: If the io_context is run on multiple threads in the future,
// the state variables retry_count_, is_send_offer_, has_is_exist_user_flag_ need to be std::atomic or protected by an appropriate synchronization mechanism.
class AyameClient : public std::enable_shared_from_this<AyameClient>,
                    public RTCMessageSender,
                    public StatsCollector {
  AyameClient(boost::asio::io_context& ioc,
              RTCManager* manager,
              AyameClientConfig config);

 public:
  static std::shared_ptr<AyameClient> Create(boost::asio::io_context& ioc,
                                             RTCManager* manager,
                                             AyameClientConfig config) {
    return std::shared_ptr<AyameClient>(
        new AyameClient(ioc, manager, std::move(config)));
  }
  ~AyameClient();

  void Reset();
  void Connect();
  void Close();

  void GetStats(std::function<void(
                    const webrtc::scoped_refptr<const webrtc::RTCStatsReport>&)>
                    callback) override;

 private:
  void ReconnectAfter();
  void OnWatchdogExpired();

 private:
  void DoRead();
  void DoRegister();
  void DoSendPong();

 private:
  void OnConnect(boost::system::error_code ec);
  void OnClose(boost::system::error_code ec);
  void OnRead(boost::system::error_code ec,
              std::size_t bytes_transferred,
              std::string text);

 private:
  // Callback from WebRTC
  // These are called from a different thread, so handle with care
  void OnIceConnectionStateChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceCandidate(const std::string sdp_mid,
                      const int sdp_mlineindex,
                      const std::string sdp) override;

 private:
  void DoIceConnectionStateChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);

 private:
  boost::asio::io_context& ioc_;
  std::unique_ptr<Websocket> ws_;

  std::atomic_bool destructed_ = {false};

  RTCManager* manager_;
  std::shared_ptr<RTCConnection> connection_;
  AyameClientConfig config_;

  int retry_count_ = 0;
  webrtc::PeerConnectionInterface::IceConnectionState rtc_state_ =
      webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionNew;

  WatchDog watchdog_;

  bool is_send_offer_ = false;
  bool has_is_exist_user_flag_ = false;

  webrtc::PeerConnectionInterface::IceServers ice_servers_;
};

#endif  // AYAME_CLIENT_H_
