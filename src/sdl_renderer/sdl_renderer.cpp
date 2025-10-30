#include "sdl_renderer.h"

#include <array>
#include <cctype>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <vector>

// WebRTC
#include <api/video/i420_buffer.h>
#include <rtc_base/logging.h>
#include <third_party/libyuv/include/libyuv/convert_from.h>
#include <third_party/libyuv/include/libyuv/video_common.h>

#define STD_ASPECT 1.33
#define WIDE_ASPECT 1.78
// Low latency: remove fixed 30 FPS frame interval sleep, and let the decoder reach the driver to present
// If you need to limit the speed, you can change it to a smaller sleep (e.g. 1ms)
constexpr Uint32 kActiveFrameIntervalMs = 8;   // ~120 FPS upper bound when video available
constexpr Uint32 kIdleFrameIntervalMs = 16;    // ~60 FPS when waiting for frames/overlays

SDLRenderer::SDLRenderer(int width, int height, bool fullscreen)
    : running_(true),
      window_(nullptr),
      renderer_(nullptr),
      dispatch_(nullptr),
      width_(width),
      height_(height),
      rows_(1),
      cols_(1),
      audio_device_(0),
      audio_stream_(nullptr),
      keyboard_hook_(std::make_unique<sdl_hook::KeyboardHookManager>()) {
  // Unless the caller explicitly overrides SDL_HINT_RENDER_VSYNC, enable VSYNC to prevent tearing
  const char* vsync_hint = SDL_GetHint(SDL_HINT_RENDER_VSYNC);
  if (vsync_hint == nullptr) {
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
  }

  // Initialize VIDEO first (critical for display)
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": SDL_Init VIDEO failed "
                      << SDL_GetError();
    return;
  }

  // Try to initialize AUDIO separately (optional, failure is non-fatal)
  RTC_LOG(LS_INFO) << __FUNCTION__
                   << ": Attempting to initialize audio subsystem...";
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": SDL_InitSubSystem(AUDIO) failed: "
                      << SDL_GetError();
    RTC_LOG(LS_WARNING) << __FUNCTION__ << ": Continuing without audio support";
  } else {
    RTC_LOG(LS_INFO) << __FUNCTION__
                     << ": Audio subsystem initialized successfully";
  }

  window_ = SDL_CreateWindow("Momo WebRTC Native Client", width_, height_,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (window_ == nullptr) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": SDL_CreateWindow failed "
                      << SDL_GetError();
    return;
  }

  if (fullscreen) {
    SetFullScreen(true);
  }

#if defined(__APPLE__)
  // Apple Silicon Mac + macOS 11.0,
  // if SDL_CreateRenderer is not called from the main thread, an error will occur
  renderer_ = SDL_CreateRenderer(window_, NULL);
  if (renderer_ == nullptr) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": SDL_CreateRenderer failed "
                      << SDL_GetError();
    return;
  }
#endif

  // Enable blending to correctly draw semi-transparent overlays
  if (renderer_) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
  }

  // Initialize audio device for playback (only if audio subsystem is available)
  RTC_LOG(LS_INFO) << __FUNCTION__
                   << ": Checking if audio subsystem is initialized...";
  if (SDL_WasInit(SDL_INIT_AUDIO)) {
    RTC_LOG(LS_INFO)
        << __FUNCTION__
        << ": Audio subsystem is available, opening audio device...";

    // Explicitly build and bind the logical device and stream of SDL3
    SDL_AudioSpec input_spec;
    SDL_zero(input_spec);
    input_spec.freq = SDLRenderer::kAudioSampleRate;
    input_spec.format = SDLRenderer::kAudioFormat;
    input_spec.channels = static_cast<int>(SDLRenderer::kAudioChannels);

    audio_device_ =
        SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (audio_device_ == 0) {
      RTC_LOG(LS_ERROR) << __FUNCTION__
                        << ": Failed to open audio device: " << SDL_GetError();
    } else {
      SDL_AudioSpec device_spec;
      SDL_zero(device_spec);
      if (!SDL_GetAudioDeviceFormat(audio_device_, &device_spec, nullptr)) {
        RTC_LOG(LS_ERROR) << __FUNCTION__
                          << ": Failed to query audio device format: "
                          << SDL_GetError();
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
      } else {
        audio_stream_ = SDL_CreateAudioStream(&input_spec, &device_spec);
        if (audio_stream_ == nullptr) {
          RTC_LOG(LS_ERROR)
              << __FUNCTION__
              << ": Failed to create audio stream: " << SDL_GetError();
          SDL_CloseAudioDevice(audio_device_);
          audio_device_ = 0;
        } else if (!SDL_BindAudioStream(audio_device_, audio_stream_)) {
          RTC_LOG(LS_ERROR)
              << __FUNCTION__
              << ": Failed to bind audio stream: " << SDL_GetError();
          SDL_DestroyAudioStream(audio_stream_);
          audio_stream_ = nullptr;
          SDL_CloseAudioDevice(audio_device_);
          audio_device_ = 0;
        } else {
          RTC_LOG(LS_INFO) << __FUNCTION__
                           << ": Audio device opened successfully, device ID: "
                           << audio_device_;
          RTC_LOG(LS_INFO) << __FUNCTION__
                           << ": Device spec - freq: " << device_spec.freq
                           << "Hz, channels: " << device_spec.channels
                           << ", format: " << device_spec.format;

          if (!SDL_SetAudioStreamGain(audio_stream_, 1.0f)) {
            RTC_LOG(LS_WARNING)
                << __FUNCTION__
                << ": Failed to set audio stream gain: " << SDL_GetError();
          }
          if (!SDL_SetAudioDeviceGain(audio_device_, 1.0f)) {
            RTC_LOG(LS_WARNING)
                << __FUNCTION__
                << ": Failed to set audio device gain: " << SDL_GetError();
          }

          if (SDL_ResumeAudioDevice(audio_device_)) {
            RTC_LOG(LS_INFO)
                << __FUNCTION__
                << ": Audio device ready (48kHz input, stereo S16)";
          } else {
            RTC_LOG(LS_ERROR)
                << __FUNCTION__
                << ": Failed to resume audio device: " << SDL_GetError();
          }
        }
      }
    }
  } else {
    RTC_LOG(LS_WARNING)
        << __FUNCTION__
        << ": Audio subsystem not available, running video-only";
  }

  // Initialize keyboard hook for system key interception
  keyboard_hook_->Initialize();
  keyboard_hook_->SetSDLWindow(window_);

  thread_ = SDL_CreateThread(SDLRenderer::RenderThreadExec, "Render", this);
}

SDLRenderer::~SDLRenderer() {
  running_ = false;

  // Shutdown keyboard hook
  if (keyboard_hook_) {
    keyboard_hook_->Shutdown();
  }

  // Clean up audio
  if (audio_stream_) {
    SDL_DestroyAudioStream(audio_stream_);
    audio_stream_ = nullptr;
  }
  if (audio_device_) {
    SDL_CloseAudioDevice(audio_device_);
    audio_device_ = 0;
  }

  int ret = 0;
  SDL_WaitThread(thread_, &ret);
  if (ret != 0) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": SDL Thread error:" << ret;
  }
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
  }
  if (window_) {
    SDL_DestroyWindow(window_);
  }
  SDL_Quit();
}

bool SDLRenderer::IsFullScreen() {
  return SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN;
}

void SDLRenderer::SetFullScreen(bool fullscreen) {
  SDL_SetWindowFullscreen(window_, fullscreen);
  if (fullscreen) {
    SDL_HideCursor();
  } else {
    SDL_ShowCursor();
  }
}

void SDLRenderer::PollEvent() {
  SDL_Event e;
  // Call it from the main thread
  while (SDL_PollEvent(&e)) {
    // Update keyboard hook on mouse motion, focus changes, and mouse enter/leave
    if ((e.type == SDL_EVENT_MOUSE_MOTION || 
         e.type == SDL_EVENT_WINDOW_FOCUS_GAINED || 
         e.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
         e.type == SDL_EVENT_WINDOW_MOUSE_ENTER ||
         e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) && keyboard_hook_) {
      keyboard_hook_->UpdateMouseTracking();
    }

    // Before processing internally, give priority to the event hook; if the callback returns true, it is considered that the event has been consumed
    if (event_hook_cb_) {
      if (event_hook_cb_(e)) {
        continue;
      }
    }
    if (e.type == SDL_EVENT_WINDOW_RESIZED &&
        e.window.windowID == SDL_GetWindowID(window_)) {
      webrtc::MutexLock lock(&sinks_lock_);
      width_ = e.window.data1;
      height_ = e.window.data2;
      SetOutlines();
    }

    if (e.type == SDL_EVENT_KEY_UP) {
      // Adjusted to Ctrl + Alt + Shift + F/Q combination keys
      const SDL_Keymod mods = SDL_GetModState();
      const SDL_Keymod need = (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_SHIFT);
      const bool combo = ((mods & need) == need);
      if (combo && e.key.key == SDLK_F) {
        SetFullScreen(!IsFullScreen());
      } else if (combo && e.key.key == SDLK_Q) {
        std::raise(SIGTERM);
      }
    }
    if (e.type == SDL_EVENT_QUIT) {
      std::raise(SIGTERM);
    }
  }
}

void SDLRenderer::SetDispatchFunction(
    std::function<void(std::function<void()>)> dispatch) {
  webrtc::MutexLock lock(&sinks_lock_);
  dispatch_ = std::move(dispatch);
}

int SDLRenderer::RenderThreadExec(void* data) {
  return ((SDLRenderer*)data)->RenderThread();
}

int SDLRenderer::RenderThread() {
#if !defined(__APPLE__)
  renderer_ = SDL_CreateRenderer(window_, NULL);
  if (renderer_ == nullptr) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": SDL_CreateRenderer failed "
                      << SDL_GetError();
    return 1;
  }
#endif

  // Enable blending to correctly draw semi-transparent overlays
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);

  uint32_t start_time, duration;
  while (running_) {
    start_time = SDL_GetTicks();
    bool drew_frame = false;
    bool has_sinks = false;
    {
      webrtc::MutexLock lock(&sinks_lock_);
      has_sinks = !sinks_.empty();
      SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
      SDL_RenderClear(renderer_);
      bool has_valid_frame = false;
      for (const VideoTrackSinkVector::value_type& sinks : sinks_) {
        Sink* sink = sinks.second.get();

        webrtc::MutexLock frame_lock(sink->GetMutex());

        if (!sink->GetOutlineChanged())
          continue;

        int width = sink->GetFrameWidth();
        int height = sink->GetFrameHeight();

        if (width == 0 || height == 0)
          continue;

        has_valid_frame = true;
        auto& cache = sink_textures_[sink];
        if (!cache.texture || cache.width != width || cache.height != height) {
          if (cache.texture) {
            SDL_DestroyTexture(cache.texture);
          }
          cache.texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STREAMING, width, height);
          if (!cache.texture) {
            RTC_LOG(LS_ERROR) << __FUNCTION__
                              << ": SDL_CreateTexture failed " << SDL_GetError();
            cache.width = cache.height = 0;
            continue;
          }
          SDL_SetTextureBlendMode(cache.texture, SDL_BLENDMODE_BLEND);
          cache.width = width;
          cache.height = height;
        }

        uint8_t* src_pixels = sink->GetImage();
        if (!src_pixels) {
          continue;
        }

        void* dst_pixels = nullptr;
        int dst_pitch = 0;
        if (SDL_LockTexture(cache.texture, nullptr, &dst_pixels, &dst_pitch) != 0) {
          SDL_FlushRenderer(renderer_);
          if (SDL_LockTexture(cache.texture, nullptr, &dst_pixels, &dst_pitch) != 0) {
            RTC_LOG(LS_ERROR) << __FUNCTION__
                              << ": SDL_LockTexture failed " << SDL_GetError();
            continue;
          }
        }

        const int src_stride = width * 4;
        uint8_t* dst = static_cast<uint8_t*>(dst_pixels);
        for (int row = 0; row < height; ++row) {
          std::memcpy(dst + row * dst_pitch, src_pixels + row * src_stride, src_stride);
        }
        SDL_UnlockTexture(cache.texture);

        SDL_FRect image_rect = {0, 0, static_cast<float>(width), static_cast<float>(height)};
        SDL_FRect draw_rect = {static_cast<float>(sink->GetOffsetX()),
                               static_cast<float>(sink->GetOffsetY()),
                               static_cast<float>(sink->GetWidth()),
                               static_cast<float>(sink->GetHeight())};

        // flip (self-image, etc.)
        //SDL_RenderTextureRotated(renderer_, texture, &image_rect, &draw_rect, 0, nullptr, SDL_FLIP_HORIZONTAL);
        SDL_RenderTexture(renderer_, cache.texture, &image_rect, &draw_rect);
      }
      if (!has_valid_frame) {
        DrawHomageText(renderer_);
      } else {
        drew_frame = true;
      }
      drew_frame = drew_frame || has_valid_frame;
      // Overlay custom overlays before rendering (mouse image, virtual keyboard, controller, RDP toolbar, etc.)
      if (overlay_render_cb_) {
        overlay_render_cb_(renderer_);
        drew_frame = true;
      }
      SDL_RenderPresent(renderer_);

      for (auto it = sink_textures_.begin(); it != sink_textures_.end();) {
        bool exists = std::any_of(
            sinks_.begin(), sinks_.end(),
            [&](const VideoTrackSinkVector::value_type& pair) {
              return pair.second.get() == it->first;
            });
        if (!exists) {
          if (it->second.texture) {
            SDL_DestroyTexture(it->second.texture);
          }
          it = sink_textures_.erase(it);
        } else {
          ++it;
        }
      }

      if (dispatch_) {
        dispatch_(std::bind(&SDLRenderer::PollEvent, this));
      }
    }
    duration = SDL_GetTicks() - start_time;
    const Uint32 target_interval = drew_frame
                                       ? kActiveFrameIntervalMs
                                       : (has_sinks ? kActiveFrameIntervalMs
                                                    : kIdleFrameIntervalMs);
    if (duration < target_interval) {
      SDL_Delay(target_interval - duration);
    }
  }

  for (auto& entry : sink_textures_) {
    if (auto tex = entry.second.texture) {
      SDL_DestroyTexture(tex);
    }
  }
  sink_textures_.clear();

  SDL_DestroyRenderer(renderer_);
  renderer_ = nullptr;

  return 0;
}

SDLRenderer::Sink::Sink(SDLRenderer* renderer,
                        webrtc::VideoTrackInterface* track)
    : renderer_(renderer),
      track_(track),
      outline_offset_x_(0),
      outline_offset_y_(0),
      outline_width_(0),
      outline_height_(0),
      outline_changed_(false),
      input_width_(0),
      input_height_(0),
      scaled_(false),
      width_(0),
      height_(0) {
  track_->AddOrUpdateSink(this, webrtc::VideoSinkWants());
}

SDLRenderer::Sink::~Sink() {
  track_->RemoveSink(this);
}

void SDLRenderer::Sink::OnFrame(const webrtc::VideoFrame& frame) {
  if (outline_width_ == 0 || outline_height_ == 0)
    return;
  if (frame.width() == 0 || frame.height() == 0)
    return;
  webrtc::MutexLock lock(GetMutex());
  if (outline_changed_ || frame.width() != input_width_ ||
      frame.height() != input_height_) {
    int width, height;
    float frame_aspect = (float)frame.width() / (float)frame.height();
    if (frame_aspect > outline_aspect_) {
      width = outline_width_;
      height = width / frame_aspect;
      offset_x_ = 0;
      offset_y_ = (outline_height_ - height) / 2;
    } else {
      height = outline_height_;
      width = height * frame_aspect;
      offset_x_ = (outline_width_ - width) / 2;
      offset_y_ = 0;
    }
    if (width_ != width || height_ != height) {
      width_ = width;
      height_ = height;
    }
    input_width_ = frame.width();
    input_height_ = frame.height();
    scaled_ = width_ < input_width_;
    if (scaled_) {
      image_.reset(new uint8_t[width_ * height_ * 4]);
    } else {
      image_.reset(new uint8_t[input_width_ * input_height_ * 4]);
    }
    RTC_LOG(LS_VERBOSE) << __FUNCTION__ << ": scaled_=" << scaled_;
    outline_changed_ = false;
  }
  webrtc::scoped_refptr<webrtc::I420BufferInterface> buffer_if;
  if (scaled_) {
    webrtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(width_, height_);
    buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());
    if (frame.rotation() != webrtc::kVideoRotation_0) {
      buffer = webrtc::I420Buffer::Rotate(*buffer, frame.rotation());
    }
    buffer_if = buffer;
  } else {
    buffer_if = frame.video_frame_buffer()->ToI420();
  }
  libyuv::ConvertFromI420(
      buffer_if->DataY(), buffer_if->StrideY(), buffer_if->DataU(),
      buffer_if->StrideU(), buffer_if->DataV(), buffer_if->StrideV(),
      image_.get(), (scaled_ ? width_ : input_width_) * 4, buffer_if->width(),
      buffer_if->height(), libyuv::FOURCC_ARGB);
}

void SDLRenderer::Sink::SetOutlineRect(int x, int y, int width, int height) {
  outline_offset_x_ = x;
  outline_offset_y_ = y;
  if (outline_width_ == width && outline_height_ == height) {
    return;
  }
  webrtc::MutexLock lock(GetMutex());
  offset_y_ = 0;
  offset_x_ = 0;
  outline_width_ = width;
  outline_height_ = height;
  outline_aspect_ = (float)outline_width_ / (float)outline_height_;
  outline_changed_ = true;
}

webrtc::Mutex* SDLRenderer::Sink::GetMutex() {
  return &frame_params_lock_;
}

bool SDLRenderer::Sink::GetOutlineChanged() {
  return !outline_changed_;
}

int SDLRenderer::Sink::GetOffsetX() {
  return outline_offset_x_ + offset_x_;
}

int SDLRenderer::Sink::GetOffsetY() {
  return outline_offset_y_ + offset_y_;
}

int SDLRenderer::Sink::GetFrameWidth() {
  return scaled_ ? width_ : input_width_;
}

int SDLRenderer::Sink::GetFrameHeight() {
  return scaled_ ? height_ : input_height_;
}

int SDLRenderer::Sink::GetInputFrameWidth() {
  return input_width_;
}

int SDLRenderer::Sink::GetInputFrameHeight() {
  return input_height_;
}

int SDLRenderer::Sink::GetWidth() {
  return width_;
}

int SDLRenderer::Sink::GetHeight() {
  return height_;
}

uint8_t* SDLRenderer::Sink::GetImage() {
  return image_.get();
}

void SDLRenderer::SetOutlines() {
  float window_aspect = (float)width_ / (float)height_;
  bool window_is_wide = window_aspect > ((STD_ASPECT + WIDE_ASPECT) / 2.0);
  float frame_aspect = window_is_wide ? WIDE_ASPECT : STD_ASPECT;
  int rows = 1;
  int cols = 1;
  if (window_aspect >= frame_aspect) {
    int times = std::floor(window_aspect / frame_aspect);
    if (times < 1)
      times = 1;
    while (rows * cols < sinks_.size()) {
      if (times < (cols / rows)) {
        rows++;
      } else {
        cols++;
      }
    }
  } else {
    int times = std::floor(frame_aspect / window_aspect);
    if (times < 1)
      times = 1;
    while (rows * cols < sinks_.size()) {
      if (times < (rows / cols)) {
        cols++;
      } else {
        rows++;
      }
    }
  }
  RTC_LOG(LS_VERBOSE) << __FUNCTION__ << " rows:" << rows << " cols:" << cols;
  int outline_width = std::floor(width_ / cols);
  int outline_height = std::floor(height_ / rows);
  int sinks_count = sinks_.size();
  for (int i = 0; i < sinks_count; i++) {
    Sink* sink = sinks_[i].second.get();
    int offset_x = outline_width * (i % cols);
    int offset_y = outline_height * std::floor(i / cols);
    sink->SetOutlineRect(offset_x, offset_y, outline_width, outline_height);
    RTC_LOG(LS_VERBOSE) << __FUNCTION__ << " offset_x:" << offset_x
                        << " offset_y:" << offset_y
                        << " outline_width:" << outline_width
                        << " outline_height:" << outline_height;
  }
  rows_ = rows;
  cols_ = cols;
}

void SDLRenderer::AddTrack(webrtc::VideoTrackInterface* track) {
  std::unique_ptr<Sink> sink(new Sink(this, track));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::make_pair(track, std::move(sink)));
  SetOutlines();
}

void SDLRenderer::RemoveTrack(webrtc::VideoTrackInterface* track) {
  webrtc::MutexLock lock(&sinks_lock_);
  for (auto it = sinks_.begin(); it != sinks_.end();) {
    if (it->first == track) {
      Sink* sink_ptr = it->second.get();
      auto tex_it = sink_textures_.find(sink_ptr);
      if (tex_it != sink_textures_.end()) {
        if (tex_it->second.texture) {
          SDL_DestroyTexture(tex_it->second.texture);
        }
        sink_textures_.erase(tex_it);
      }
      it = sinks_.erase(it);
    } else {
      ++it;
    }
  }
  SetOutlines();
}

// Set the overlay rendering callback
void SDLRenderer::SetOverlayRenderCallback(
    std::function<void(SDL_Renderer*)> cb) {
  overlay_render_cb_ = std::move(cb);
}

// Set the event hook
void SDLRenderer::SetEventHook(std::function<bool(const SDL_Event&)> cb) {
  event_hook_cb_ = std::move(cb);
}

// Get the primary video drawing rectangle and source frame size (based on the first track)
bool SDLRenderer::GetPrimaryVideoRect(int& x,
                                      int& y,
                                      int& w,
                                      int& h,
                                      int& frame_w,
                                      int& frame_h) {
  webrtc::MutexLock lock(&sinks_lock_);
  if (sinks_.empty()) {
    return false;
  }
  Sink* sink = sinks_.front().second.get();
  // To avoid race conditions, hold the mutex when reading the frame size
  {
    webrtc::MutexLock frame_lock(sink->GetMutex());
    x = sink->GetOffsetX();
    y = sink->GetOffsetY();
    w = sink->GetWidth();
    h = sink->GetHeight();
    // Return the actual size of the source input frame, for absolute coordinate mapping
    frame_w = sink->GetInputFrameWidth();
    frame_h = sink->GetInputFrameHeight();
    if (w == 0 || h == 0 || frame_w == 0 || frame_h == 0) {
      return false;
    }
  }
  return true;
}

void SDLRenderer::DrawHomageText(SDL_Renderer* renderer) {
  if (!renderer)
    return;
  const char* text = "A TRIBUTE TO HONPC";
  const int len = static_cast<int>(std::strlen(text));
  if (len <= 0)
    return;

  int w = 0, h = 0;
  SDL_GetRenderOutputSize(renderer, &w, &h);
  if (w <= 0 || h <= 0)
    return;

  float target_width = static_cast<float>(w) * 0.75f;
  float char_scale = target_width / (len * 6.0f);
  if (char_scale <= 0.0f)
    char_scale = 1.0f;
  float char_width = 6.0f * char_scale;
  float char_height = 7.0f * char_scale;
  float start_x = (static_cast<float>(w) - char_width * len) * 0.5f;
  float start_y = (static_cast<float>(h) - char_height) * 0.5f;

  auto glyph = [](char c) -> const uint8_t* {
    switch (c) {
      case 'A': {
        static const uint8_t g[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x00};
        return g;
      }
      case 'B': {
        static const uint8_t g[7] = {0x0F, 0x11, 0x0F, 0x11, 0x11, 0x0F, 0x00};
        return g;
      }
      case 'C': {
        static const uint8_t g[7] = {0x0E, 0x11, 0x01, 0x01, 0x11, 0x0E, 0x00};
        return g;
      }
      case 'E': {
        static const uint8_t g[7] = {0x1F, 0x01, 0x0F, 0x01, 0x01, 0x1F, 0x00};
        return g;
      }
      case 'H': {
        static const uint8_t g[7] = {0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00};
        return g;
      }
      case 'I': {
        static const uint8_t g[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x00};
        return g;
      }
      case 'N': {
        static const uint8_t g[7] = {0x11, 0x13, 0x15, 0x19, 0x11, 0x11, 0x00};
        return g;
      }
      case 'O': {
        static const uint8_t g[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00};
        return g;
      }
      case 'P': {
        static const uint8_t g[7] = {0x0F, 0x11, 0x0F, 0x01, 0x01, 0x01, 0x00};
        return g;
      }
      case 'R': {
        static const uint8_t g[7] = {0x0F, 0x11, 0x0F, 0x05, 0x09, 0x11, 0x00};
        return g;
      }
      case 'T': {
        static const uint8_t g[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00};
        return g;
      }
      case 'U': {
        static const uint8_t g[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00};
        return g;
      }
      case ' ': {
        static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        return g;
      }
      default:
        return nullptr;
    }
  };

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  float x = start_x;
  for (int i = 0; i < len; ++i) {
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(text[i])));
    const uint8_t* g = glyph(c);
    if (!g) {
      x += char_width;
      continue;
    }
    for (int row = 0; row < 7; ++row) {
      uint8_t bits = g[row];
      for (int col = 0; col < 5; ++col) {
        if (bits & (1u << col)) {
          SDL_FRect px_rect{x + col * char_scale,
                            start_y + row * char_scale,
                            char_scale * 0.9f, char_scale * 0.9f};
          SDL_RenderFillRect(renderer, &px_rect);
        }
      }
    }
    x += char_width;
  }
}

// Audio track management (Audio track support)
void SDLRenderer::AddAudioTrack(webrtc::AudioTrackInterface* track) {
  std::unique_ptr<AudioSink> sink(new AudioSink(this, track));
  webrtc::MutexLock lock(&audio_sinks_lock_);
  audio_sinks_.push_back(std::make_pair(track, std::move(sink)));
  RTC_LOG(LS_INFO) << __FUNCTION__ << ": Added audio track";
}

void SDLRenderer::RemoveAudioTrack(webrtc::AudioTrackInterface* track) {
  webrtc::MutexLock lock(&audio_sinks_lock_);
  audio_sinks_.erase(
      std::remove_if(audio_sinks_.begin(), audio_sinks_.end(),
                     [track](const AudioTrackSinkVector::value_type& sink) {
                       return sink.first == track;
                     }),
      audio_sinks_.end());
  RTC_LOG(LS_INFO) << __FUNCTION__ << ": Removed audio track";
}

// AudioSink implementation
SDLRenderer::AudioSink::AudioSink(SDLRenderer* renderer,
                                  webrtc::AudioTrackInterface* track)
    : renderer_(renderer), track_(track) {
  track_->AddSink(this);
  RTC_LOG(LS_INFO) << __FUNCTION__ << ": AudioSink created";
}

SDLRenderer::AudioSink::~AudioSink() {
  track_->RemoveSink(this);
  RTC_LOG(LS_INFO) << __FUNCTION__ << ": AudioSink destroyed";
}

void SDLRenderer::AudioSink::OnData(const void* audio_data,
                                    int bits_per_sample,
                                    int sample_rate,
                                    size_t number_of_channels,
                                    size_t number_of_frames) {
  if (!renderer_->audio_stream_) {
    return;
  }

  // The audio from WebRTC is always passed in S16
  if (bits_per_sample != 16) {
    RTC_LOG(LS_WARNING) << __FUNCTION__
                        << ": Unexpected bits_per_sample: " << bits_per_sample;
    return;
  }

  // For easy debugging, output information only for the first frame
  static bool first_audio = true;
  if (first_audio) {
    RTC_LOG(LS_INFO) << __FUNCTION__
                     << ": First audio frame - sample_rate: " << sample_rate
                     << "Hz, channels: " << number_of_channels
                     << ", frames: " << number_of_frames;
    first_audio = false;
  }

  const int16_t* input = static_cast<const int16_t*>(audio_data);
  size_t samples_per_channel = number_of_frames;
  const int16_t* playback_data = nullptr;
  size_t playback_frames = 0;

  if (number_of_channels == SDLRenderer::kAudioChannels &&
      sample_rate == SDLRenderer::kAudioSampleRate) {
    // Already the target format, so just stream it
    const int byte_count = static_cast<int>(
        samples_per_channel * number_of_channels * sizeof(int16_t));
    if (!SDL_PutAudioStreamData(renderer_->audio_stream_, input, byte_count)) {
      RTC_LOG(LS_ERROR) << __FUNCTION__ << ": SDL_PutAudioStreamData failed: "
                        << SDL_GetError();
    }
  } else {
    // If the sample rate or number of channels are different, convert it
    std::vector<int16_t> converted_data;

    // First, align the number of channels
    const int16_t* channel_converted = input;
    std::vector<int16_t> stereo_temp;

    if (number_of_channels == 1 && SDLRenderer::kAudioChannels == 2) {
      // If mono, stereoize it with equal left and right values
      stereo_temp.resize(samples_per_channel * 2);
      for (size_t i = 0; i < samples_per_channel; i++) {
        stereo_temp[i * 2] = input[i];
        stereo_temp[i * 2 + 1] = input[i];
      }
      channel_converted = stereo_temp.data();
    }

    // Next, interpolate the sample rate
    if (sample_rate != SDLRenderer::kAudioSampleRate) {
      // Simplistically resample using linear interpolation
      double ratio =
          static_cast<double>(SDLRenderer::kAudioSampleRate) / sample_rate;
      size_t output_frames = static_cast<size_t>(samples_per_channel * ratio);
      converted_data.resize(output_frames * SDLRenderer::kAudioChannels);

      for (size_t i = 0; i < output_frames; i++) {
        double src_pos = i / ratio;
        size_t src_idx = static_cast<size_t>(src_pos);
        double frac = src_pos - src_idx;

        if (src_idx + 1 < samples_per_channel) {
          // Linear interpolation
          for (size_t ch = 0; ch < SDLRenderer::kAudioChannels; ch++) {
            int16_t s0 =
                channel_converted[src_idx * SDLRenderer::kAudioChannels + ch];
            int16_t s1 =
                channel_converted[(src_idx + 1) * SDLRenderer::kAudioChannels +
                                  ch];
            converted_data[i * SDLRenderer::kAudioChannels + ch] =
                static_cast<int16_t>(s0 + frac * (s1 - s0));
          }
        } else {
          // The end is copied without interpolation
          for (size_t ch = 0; ch < SDLRenderer::kAudioChannels; ch++) {
            converted_data[i * SDLRenderer::kAudioChannels + ch] =
                channel_converted[src_idx * SDLRenderer::kAudioChannels + ch];
          }
        }
      }

      const int byte_count =
          static_cast<int>(converted_data.size() * sizeof(int16_t));
      if (!SDL_PutAudioStreamData(renderer_->audio_stream_,
                                  converted_data.data(), byte_count)) {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << ": SDL_PutAudioStreamData failed: "
                          << SDL_GetError();
      }
    } else {
      // If only channel conversion is needed, do it
      const int byte_count = static_cast<int>(
          samples_per_channel * SDLRenderer::kAudioChannels * sizeof(int16_t));
      if (!SDL_PutAudioStreamData(renderer_->audio_stream_, channel_converted,
                                  byte_count)) {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << ": SDL_PutAudioStreamData failed: "
                          << SDL_GetError();
      }
    }
  }
}
