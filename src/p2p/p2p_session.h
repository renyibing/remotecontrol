#ifndef P2P_SESSION_H_
#define P2P_SESSION_H_

#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

// Boost
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>

#include "p2p_websocket_session.h"
#include "rtc/rtc_manager.h"
#include "util.h"

struct P2PSessionConfig {
  bool no_google_stun = false;
  std::string doc_root;
};

// Class for processing one HTTP request
class P2PSession : public std::enable_shared_from_this<P2PSession> {
  P2PSession(boost::asio::io_context& ioc,
             boost::asio::ip::tcp::socket socket,
             RTCManager* rtc_manager,
             P2PSessionConfig config);

 public:
  static std::shared_ptr<P2PSession> Create(boost::asio::io_context& ioc,
                                            boost::asio::ip::tcp::socket socket,
                                            RTCManager* rtc_manager,
                                            P2PSessionConfig config) {
    return std::shared_ptr<P2PSession>(
        new P2PSession(ioc, std::move(socket), rtc_manager, std::move(config)));
  }
  void Run();

  std::shared_ptr<RTCConnection> GetRTCConnection() const;

 private:
  void DoRead();
  void OnRead(boost::system::error_code ec, std::size_t bytes_transferred);

  void HandleRequest();

  template <class Body, class Fields>
  void SendResponse(boost::beast::http::response<Body, Fields> msg) {
    auto sp = std::make_shared<boost::beast::http::response<Body, Fields>>(
        std::move(msg));

    // The msg object needs to live until the write is complete, so put it in the member and extend the lifetime
    res_ = sp;

    // Write the response
    boost::beast::http::async_write(
        socket_, *sp,
        boost::asio::bind_executor(
            strand_, std::bind(&P2PSession::OnWrite, shared_from_this(),
                               std::placeholders::_1, std::placeholders::_2,
                               sp->need_eof())));
  }

  void OnWrite(boost::system::error_code ec,
               std::size_t bytes_transferred,
               bool close);
  void DoClose();

 private:
  boost::asio::io_context& ioc_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::strand<boost::asio::ip::tcp::socket::executor_type> strand_;
  boost::beast::flat_buffer buffer_;
  boost::beast::http::request<boost::beast::http::string_body> req_;
  std::shared_ptr<void> res_;

  RTCManager* rtc_manager_;
  P2PSessionConfig config_;

  std::shared_ptr<P2PWebsocketSession> ws_session_;
};

#endif  // P2P_SESSION_H_
