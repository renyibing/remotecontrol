#ifndef RTC_DATA_MANAGER_H_
#define RTC_DATA_MANAGER_H_

// WebRTC
#include <api/data_channel_interface.h>

class RTCDataManager {
 public:
  virtual ~RTCDataManager() = default;
  // DataChannel always transitions to kClosed state, so OnRemove is not needed
  virtual void OnDataChannel(
      webrtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) = 0;
};

#endif
