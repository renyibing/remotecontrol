#ifndef SDL_RENDERER_H_
#define SDL_RENDERER_H_

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

// SDL
#include <SDL3/SDL.h>

// Boost
#include <boost/asio.hpp>

// WebRTC
#include <api/media_stream_interface.h>
#include <api/scoped_refptr.h>
#include <api/video/video_frame.h>
#include <api/video/video_sink_interface.h>
#include <modules/audio_processing/include/audio_processing.h>
#include <rtc/video_track_receiver.h>
#include <rtc_base/synchronization/mutex.h>

// Keyboard hook for system-level key interception
#include "keyboard_hook.h"

// Forward declarations for audio support
namespace webrtc {
class AudioTrackInterface;
}

class SDLRenderer : public VideoTrackReceiver {
 public:
  SDLRenderer(int width, int height, bool fullscreen);
  ~SDLRenderer();

  void SetDispatchFunction(std::function<void(std::function<void()>)> dispatch);

  static int RenderThreadExec(void* data);
  int RenderThread();

  void SetOutlines();
  void AddTrack(webrtc::VideoTrackInterface* track) override;
  void RemoveTrack(webrtc::VideoTrackInterface* track) override;

  // Audio track support
  void AddAudioTrack(webrtc::AudioTrackInterface* track) override;
  void RemoveAudioTrack(webrtc::AudioTrackInterface* track) override;
  // The following are the minimum entry points for remote control overlays and input capture
  // Note: only provide hooks, default is not enabled, to avoid affecting existing behavior

  // Set the post-rendering callback, for drawing overlays on top of video frames (mouse image, keyboard, controller, RDP toolbar, etc.)
  // Note: the callback should not block for too long
  void SetOverlayRenderCallback(std::function<void(SDL_Renderer*)> cb);

  // Set the event hook, intercept the SDL event before processing; if the callback returns true, it is considered that the event has been consumed, and no further processing is done
  void SetEventHook(std::function<bool(const SDL_Event&)> cb);

  // Get the primary video drawing rectangle and source frame size (based on the first track)
  // Return false if there is no available frame
  bool GetPrimaryVideoRect(int& x,
                           int& y,
                           int& w,
                           int& h,
                           int& frame_w,
                           int& frame_h);

  // Get the SDL window pointer (for window minimization, etc.)
  SDL_Window* GetWindow() { return window_; }

  bool IsFullScreen();
  void SetFullScreen(bool fullscreen);

 protected:
  static constexpr int kAudioSampleRate = 48000;
  static constexpr size_t kAudioChannels = 2;
  static constexpr SDL_AudioFormat kAudioFormat = SDL_AUDIO_S16;

  class Sink : public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    Sink(SDLRenderer* renderer, webrtc::VideoTrackInterface* track);
    ~Sink();

    void OnFrame(const webrtc::VideoFrame& frame) override;

    void SetOutlineRect(int x, int y, int width, int height);

    webrtc::Mutex* GetMutex();
    bool GetOutlineChanged();
    int GetOffsetX();
    int GetOffsetY();
    int GetFrameWidth();
    int GetFrameHeight();
    int GetInputFrameWidth();
    int GetInputFrameHeight();
    int GetWidth();
    int GetHeight();
    uint8_t* GetImage();

   private:
    SDLRenderer* renderer_;
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> track_;
    webrtc::Mutex frame_params_lock_;
    int outline_offset_x_;
    int outline_offset_y_;
    int outline_width_;
    int outline_height_;
    bool outline_changed_;
    float outline_aspect_;
    int input_width_;
    int input_height_;
    bool scaled_;
    std::unique_ptr<uint8_t[]> image_;
    int offset_x_;
    int offset_y_;
    int width_;
    int height_;
  };

  // Audio sink for receiving audio frames
  class AudioSink : public webrtc::AudioTrackSinkInterface {
   public:
    AudioSink(SDLRenderer* renderer, webrtc::AudioTrackInterface* track);
    ~AudioSink();

    void OnData(const void* audio_data,
                int bits_per_sample,
                int sample_rate,
                size_t number_of_channels,
                size_t number_of_frames) override;

   private:
    SDLRenderer* renderer_;
    webrtc::scoped_refptr<webrtc::AudioTrackInterface> track_;
  };

 private:
  void PollEvent();
  void DrawHomageText(SDL_Renderer* renderer);

  webrtc::Mutex sinks_lock_;
  typedef std::vector<
      std::pair<webrtc::VideoTrackInterface*, std::unique_ptr<Sink> > >
      VideoTrackSinkVector;
  VideoTrackSinkVector sinks_;
  struct CachedTexture {
    SDL_Texture* texture{nullptr};
    int width{0};
    int height{0};
  };
  std::unordered_map<Sink*, CachedTexture> sink_textures_;
  std::atomic<bool> running_;
  SDL_Thread* thread_;
  SDL_Window* window_;
  SDL_Renderer* renderer_;
  std::function<void(std::function<void()>)> dispatch_;
  int width_;
  int height_;
  int rows_;
  int cols_;

  // Overlay rendering callback and event hook
  std::function<void(SDL_Renderer*)> overlay_render_cb_;
  std::function<bool(const SDL_Event&)> event_hook_cb_;

  // Audio support
  webrtc::Mutex audio_sinks_lock_;
  typedef std::vector<
      std::pair<webrtc::AudioTrackInterface*, std::unique_ptr<AudioSink> > >
      AudioTrackSinkVector;
  AudioTrackSinkVector audio_sinks_;
  SDL_AudioDeviceID audio_device_;
  SDL_AudioStream* audio_stream_;

  // Keyboard hook manager for system key interception
  std::unique_ptr<sdl_hook::KeyboardHookManager> keyboard_hook_;
};

#endif
