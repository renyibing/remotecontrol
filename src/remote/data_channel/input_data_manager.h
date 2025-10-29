// Skeleton for managing input DataChannel
// Comments in Chinese, emphasizing readability

#ifndef REMOTE_DATA_CHANNEL_INPUT_DATA_MANAGER_H_
#define REMOTE_DATA_CHANNEL_INPUT_DATA_MANAGER_H_

#include <memory>
#include <vector>
#include <string>

#include <api/data_channel_interface.h>
#include <rtc_base/synchronization/mutex.h>
#include <rtc_base/copy_on_write_buffer.h>

#include "rtc/rtc_data_manager.h"

namespace remote {
namespace data_channel {

// Listen and manage input-related DataChannel
// To avoid modifying the build script, use header-only minimal implementation
class InputDataManager : public RTCDataManager, public webrtc::DataChannelObserver {
 public:
  InputDataManager() = default;
  ~InputDataManager() = default;

  // Listen for new DataChannel, filter out the channels needed for this module (such as input-reliable / input-rt)
  void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> dc) override {
    webrtc::MutexLock lock(&lock_);
    const std::string label = dc->label();
    if (label == "input-reliable") {
      reliable_ = dc;
      reliable_->RegisterObserver(this);
    } else if (label == "input-rt") {
      rt_ = dc;
      rt_->RegisterObserver(this);
    }
  }

  // Send reliable message
  bool SendReliable(const std::vector<uint8_t>& bytes) {
    webrtc::MutexLock lock(&lock_);
    if (!reliable_ || reliable_->state() != webrtc::DataChannelInterface::kOpen) {
      return false;
    }
    std::string s(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    webrtc::DataBuffer buf(webrtc::CopyOnWriteBuffer(s), reliable_binary_);
    reliable_->Send(buf);
    return true;
  }

  // Send low-latency message
  bool SendRt(const std::vector<uint8_t>& bytes) {
    webrtc::MutexLock lock(&lock_);
    if (!rt_ || rt_->state() != webrtc::DataChannelInterface::kOpen) {
      return false;
    }
    std::string s(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    webrtc::DataBuffer buf(webrtc::CopyOnWriteBuffer(s), rt_binary_);
    rt_->Send(buf);
    return true;
  }

  // DataChannelObserver interface (initial only placeholder)
  void OnStateChange() override {}
  void OnMessage(const webrtc::DataBuffer& buffer) override {
    auto cb = on_message_;
    if (cb) {
      // Pass through the original data and whether it is binary
      cb(buffer.data.cdata(), buffer.data.size(), buffer.binary);
    }
  }
  void OnBufferedAmountChange(uint64_t previous_amount) override {
    (void)previous_amount;
  }

  // Set message callback (reliable and low-latency temporarily shared)
  void SetOnMessage(std::function<void(const uint8_t*, size_t, bool)> cb) {
    on_message_ = std::move(cb);
  }

  // Set channel payload type: text (false) or binary (true)
  void SetReliableBinary(bool v) { reliable_binary_ = v; }
  void SetRtBinary(bool v) { rt_binary_ = v; }
  void SetBinaryBoth(bool v) { reliable_binary_ = v; rt_binary_ = v; }

 private:
  webrtc::Mutex lock_;
  webrtc::scoped_refptr<webrtc::DataChannelInterface> reliable_;
  webrtc::scoped_refptr<webrtc::DataChannelInterface> rt_;
  std::function<void(const uint8_t*, size_t, bool)> on_message_;
  bool reliable_binary_{false};
  bool rt_binary_{false};
};

}  // namespace data_channel
}  // namespace remote

#endif  // REMOTE_DATA_CHANNEL_INPUT_DATA_MANAGER_H_
