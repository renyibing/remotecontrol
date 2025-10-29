#ifndef VIDEO_TRACK_RECEIVER_H_
#define VIDEO_TRACK_RECEIVER_H_

#include <string>

// WebRTC
#include <api/media_stream_interface.h>

class VideoTrackReceiver {
 public:
  virtual void AddTrack(webrtc::VideoTrackInterface* track) = 0;
  virtual void RemoveTrack(webrtc::VideoTrackInterface* track) = 0;

  // Audio track support (optional - implementations can provide empty stubs if not needed)
  virtual void AddAudioTrack(webrtc::AudioTrackInterface* track) {}
  virtual void RemoveAudioTrack(webrtc::AudioTrackInterface* track) {}
};

#endif
