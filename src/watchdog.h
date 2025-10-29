#ifndef WATCH_DOG_H_
#define WATCH_DOG_H_

#include <chrono>
#include <functional>

// Boost
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

/*
Class for monitoring the timeout.

After calling Enable(), the callback function is called after a certain period of time.
When the callback occurs, the WatchDog is disabled, so if necessary, call Enable() or Reset() again.
Note that it does not work in a multi-threaded environment.
*/
class WatchDog {
 public:
  WatchDog(boost::asio::io_context& ioc, std::function<void()> callback);
  void Enable(int timeout);
  void Disable();
  void Reset();

 private:
  int timeout_;
  boost::asio::steady_timer timer_;
  std::function<void()> callback_;
};

#endif
