#ifndef UTIL_H_
#define UTIL_H_

// Boost
#include <boost/beast/core/string.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include "api/peer_connection_interface.h"
#include "momo_args.h"
#include "video_codec_info.h"

class Util {
 public:
  static void ParseArgs(int argc,
                        char* argv[],
                        bool& use_p2p,
                        bool& use_ayame,
                        bool& use_sora,
                        int& log_level,
                        MomoArgs& args);
  static std::string GenerateRandomChars();
  static std::string GenerateRandomChars(size_t length);
  static std::string GenerateRandomNumericChars(size_t length);
  static std::string IceConnectionStateToString(
      webrtc::PeerConnectionInterface::IceConnectionState state);

  // Check the MIME type from the file name extension
  static boost::beast::string_view MimeType(boost::beast::string_view path);

  // Convenient function to create a nice error response
  static boost::beast::http::response<boost::beast::http::string_body>
  BadRequest(
      const boost::beast::http::request<boost::beast::http::string_body>& req,
      boost::beast::string_view why);
  static boost::beast::http::response<boost::beast::http::string_body> NotFound(
      const boost::beast::http::request<boost::beast::http::string_body>& req,
      boost::beast::string_view target);
  static boost::beast::http::response<boost::beast::http::string_body>
  ServerError(
      const boost::beast::http::request<boost::beast::http::string_body>& req,
      boost::beast::string_view what);

 private:
  static void ShowVideoCodecs(VideoCodecInfo info);
};

// Macro to nicely output the error of boost::system::error_code
//
// if (ec)
//   return MOMO_BOOST_ERROR(ec, "onRead")
//
// Can be used in combination with return, such as:
#define MOMO_BOOST_ERROR(ec, what)                                      \
  ([&ec] {                                                              \
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " " what ": " << ec.message(); \
  }())

#endif
