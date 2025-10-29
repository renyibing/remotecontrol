#include "rtc/fake_video_capturer.h"

#if defined(USE_FAKE_CAPTURE_DEVICE)

#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

// WebRTC
#include <api/video/nv12_buffer.h>
#include <rtc_base/logging.h>
#include <third_party/libyuv/include/libyuv.h>

#include "rtc/fake_audio_capturer.h"

FakeVideoCapturer::FakeVideoCapturer(Config config)
    : sora::ScalableVideoTrackSource(config), config_(config) {
  StartCapture();
}

FakeVideoCapturer::~FakeVideoCapturer() {
  StopCapture();
}

void FakeVideoCapturer::SetAudioCapturer(
    webrtc::scoped_refptr<FakeAudioCapturer> audio_capturer) {
  std::lock_guard<std::mutex> lock(audio_capturer_mutex_);
  audio_capturer_ = std::move(audio_capturer);
}

webrtc::scoped_refptr<FakeAudioCapturer> FakeVideoCapturer::GetAudioCapturer()
    const {
  std::lock_guard<std::mutex> lock(audio_capturer_mutex_);
  return audio_capturer_;
}

void FakeVideoCapturer::StartCapture() {
  if (capture_thread_) {
    return;
  }

  stop_capture_ = false;
  frame_counter_ = 0;
  start_time_ = std::chrono::high_resolution_clock::now();

  capture_thread_ = std::make_unique<std::thread>([this] { CaptureThread(); });
}

void FakeVideoCapturer::StopCapture() {
  if (!capture_thread_) {
    return;
  }

  stop_capture_ = true;
  if (capture_thread_->joinable()) {
    capture_thread_->join();
  }
  capture_thread_.reset();
}

void FakeVideoCapturer::CaptureThread() {
  // Initialize the Blend2D image
  image_.create(config_.width, config_.height, BL_FORMAT_PRGB32);
  frame_counter_ = 0;

  while (!stop_capture_) {
    auto now = std::chrono::high_resolution_clock::now();

    // Update the image
    UpdateImage(now);

    // Convert the Blend2D image to a VideoFrameBuffer
    BLImageData data;
    BLResult result = image_.get_data(&data);
    if (result != BL_SUCCESS) {
      RTC_LOG(LS_ERROR) << "Failed to get image data from Blend2D: " << result;
      RTC_LOG(LS_ERROR) << "Stopping capture thread";
      break;
    }

    // Select the output format
    webrtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer;
    if (config_.force_nv12) {
      // Convert to NV12
      auto nv12 = webrtc::NV12Buffer::Create(config_.width, config_.height);
      libyuv::ABGRToNV12((const uint8_t*)data.pixel_data, data.stride,
                         nv12->MutableDataY(), nv12->StrideY(),
                         nv12->MutableDataUV(), nv12->StrideUV(), config_.width,
                         config_.height);
      buffer = nv12;
    } else {
      // Default is I420
      auto i420 = webrtc::I420Buffer::Create(config_.width, config_.height);
      libyuv::ABGRToI420(
          (const uint8_t*)data.pixel_data, data.stride, i420->MutableDataY(),
          i420->StrideY(), i420->MutableDataU(), i420->StrideU(),
          i420->MutableDataV(), i420->StrideV(), config_.width, config_.height);
      buffer = i420;
    }

    // Calculate the timestamp
    int64_t timestamp_us =
        std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_)
            .count();

    // Send the frame
    bool captured = OnCapturedFrame(webrtc::VideoFrame::Builder()
                                        .set_video_frame_buffer(buffer)
                                        .set_rotation(webrtc::kVideoRotation_0)
                                        .set_timestamp_us(timestamp_us)
                                        .build());

    if (captured) {
      // If the sleep time is std::chrono::milliseconds(1000 / config_.fps),
      // there is a time for waking up, so the frame rate may not be maintained.
      // So, make it a little shorter.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1000 / config_.fps - 2));
      frame_counter_ += 1;
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void FakeVideoCapturer::UpdateImage(
    std::chrono::high_resolution_clock::time_point now) {
  BLContext ctx(image_);

  ctx.set_comp_op(BL_COMP_OP_SRC_COPY);
  ctx.fill_all();

  // Draw the digital clock
  ctx.save();
  DrawDigitalClock(ctx, now);
  ctx.restore();

  ctx.save();
  DrawAnimations(ctx, now);
  ctx.restore();

  ctx.save();
  DrawBoxes(ctx, now);
  ctx.restore();

  ctx.end();
}

void FakeVideoCapturer::DrawAnimations(
    BLContext& ctx,
    std::chrono::high_resolution_clock::time_point now) {
  int width = config_.width;
  int height = config_.height;
  int fps = config_.fps;

  ctx.translate(width * 0.5, height * 0.5);  // Place the screen in the center
  ctx.rotate(-M_PI / 2);
  ctx.set_fill_style(BLRgba32(255, 255, 255));
  ctx.fill_pie(0, 0, width * 0.3, 0, 2 * M_PI);  // Make it larger

  ctx.set_fill_style(BLRgba32(160, 160, 160));
  uint32_t current_frame = frame_counter_;
  ctx.fill_pie(0, 0, width * 0.3, 0,
               (current_frame % fps) / static_cast<float>(fps) * 2 * M_PI);

  // When the circle completes, play the beep sound
  auto fake_audio_capturer = GetAudioCapturer();
  if (fake_audio_capturer) {
    // Check if it is 0 degrees
    if (current_frame % fps == 0) {
      fake_audio_capturer->TriggerBeep();
    }
  }
}

void FakeVideoCapturer::DrawBoxes(
    BLContext& ctx,
    std::chrono::high_resolution_clock::time_point now) {
  int width = config_.width;
  int height = config_.height;

  // Animation of moving boxes
  const int box_size = 50;
  const int num_boxes = 5;

  for (int i = 0; i < num_boxes; i++) {
    uint32_t current_frame = frame_counter_;
    double phase = (current_frame + i * 20) % 100 / 100.0;
    double x = phase * (width - box_size);
    double y = height * 0.5 + sin(phase * M_PI * 2) * height * 0.2;

    // Set different colors for each box
    uint32_t color = 0xFF000000;
    switch (i % 5) {
      case 0:
        color |= 0xFF0000;
        break;  // Red
      case 1:
        color |= 0x00FF00;
        break;  // Green
      case 2:
        color |= 0x0000FF;
        break;  // Blue
      case 3:
        color |= 0xFFFF00;
        break;  // Yellow
      case 4:
        color |= 0xFF00FF;
        break;  // Magenta
    }

    ctx.set_fill_style(BLRgba32(color));
    ctx.fill_rect(x, y, box_size, box_size);
  }
}

void FakeVideoCapturer::DrawDigitalClock(
    BLContext& ctx,
    std::chrono::high_resolution_clock::time_point now) {
  auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_)
          .count();

  int hours = (ms / (60 * 60 * 1000)) % 10000;  // Display up to 9999
  int minutes = (ms / (60 * 1000)) % 60;
  int seconds = (ms / 1000) % 60;
  int milliseconds = ms % 1000;

  // Parameters for the digital clock layout
  double clock_x = config_.width * 0.02;        // Align to the left
  double clock_y = config_.height * 0.02;       // Align to the top
  double digit_width = config_.width * 0.018;   // Make it smaller
  double digit_height = config_.height * 0.04;  // Make the height smaller
  double spacing = digit_width * 0.3;
  double colon_width = digit_width * 0.3;

  ctx.set_fill_style(BLRgba32(0, 255, 255));  // Cyan color

  // Display HHHH:MM:SS.mmm
  double x = clock_x;

  // Hours (4 digits)
  Draw7Segment(ctx, (hours / 1000) % 10, x, clock_y, digit_width, digit_height);
  x += digit_width + spacing;
  Draw7Segment(ctx, (hours / 100) % 10, x, clock_y, digit_width, digit_height);
  x += digit_width + spacing;
  Draw7Segment(ctx, (hours / 10) % 10, x, clock_y, digit_width, digit_height);
  x += digit_width + spacing;
  Draw7Segment(ctx, hours % 10, x, clock_y, digit_width, digit_height);
  x += digit_width + spacing;

  // Colon
  DrawColon(ctx, x, clock_y, digit_height);
  x += colon_width + spacing;

  // Minutes (2 digits)
  Draw7Segment(ctx, minutes / 10, x, clock_y, digit_width, digit_height);
  x += digit_width + spacing;
  Draw7Segment(ctx, minutes % 10, x, clock_y, digit_width, digit_height);
  x += digit_width + spacing;

  // Colon
  DrawColon(ctx, x, clock_y, digit_height);
  x += colon_width + spacing;

  // Seconds (2 digits)
  Draw7Segment(ctx, seconds / 10, x, clock_y, digit_width, digit_height);
  x += digit_width + spacing;
  Draw7Segment(ctx, seconds % 10, x, clock_y, digit_width, digit_height);
  x += digit_width + spacing;

  // Dot
  ctx.fill_circle(x + colon_width * 0.3, clock_y + digit_height * 0.8,
                  digit_height * 0.05);
  x += colon_width + spacing;

  // Milliseconds (3 digits, slightly smaller)
  double ms_digit_width = digit_width * 0.7;
  double ms_digit_height = digit_height * 0.7;

  ctx.set_fill_style(BLRgba32(200, 200, 200));  // Gray color
  Draw7Segment(ctx, (milliseconds / 100) % 10, x,
               clock_y + (digit_height - ms_digit_height) / 2, ms_digit_width,
               ms_digit_height);
  x += ms_digit_width + spacing * 0.8;
  Draw7Segment(ctx, (milliseconds / 10) % 10, x,
               clock_y + (digit_height - ms_digit_height) / 2, ms_digit_width,
               ms_digit_height);
  x += ms_digit_width + spacing * 0.8;
  Draw7Segment(ctx, milliseconds % 10, x,
               clock_y + (digit_height - ms_digit_height) / 2, ms_digit_width,
               ms_digit_height);
}

void FakeVideoCapturer::Draw7Segment(BLContext& ctx,
                                     int digit,
                                     double x,
                                     double y,
                                     double width,
                                     double height) {
  // Definition of the segments of the 7-segment display
  //  aaa
  // f   b
  //  ggg
  // e   c
  //  ddd

  double thickness = width * 0.15;
  double gap = thickness * 0.2;

  // ON/OFF of each segment (corresponding to the numbers 0-9)
  bool segments[10][7] = {
      {true, true, true, true, true, true, false},      // 0
      {false, true, true, false, false, false, false},  // 1
      {true, true, false, true, true, false, true},     // 2
      {true, true, true, true, false, false, true},     // 3
      {false, true, true, false, false, true, true},    // 4
      {true, false, true, true, false, true, true},     // 5
      {true, false, true, true, true, true, true},      // 6
      {true, true, true, false, false, false, false},   // 7
      {true, true, true, true, true, true, true},       // 8
      {true, true, true, true, false, true, true}       // 9
  };

  if (digit < 0 || digit > 9)
    return;

  // Horizontal segment (a, g, d)
  auto drawHorizontalSegment = [&](double sx, double sy) {
    BLPath path;
    path.move_to(sx + gap, sy);
    path.line_to(sx + width - gap, sy);
    path.line_to(sx + width - gap - thickness * 0.5, sy + thickness * 0.5);
    path.line_to(sx + width - gap, sy + thickness);
    path.line_to(sx + gap, sy + thickness);
    path.line_to(sx + gap + thickness * 0.5, sy + thickness * 0.5);
    path.close();
    ctx.fill_path(path);
  };

  // Vertical segment (f, b, e, c)
  auto drawVerticalSegment = [&](double sx, double sy, double sh) {
    BLPath path;
    path.move_to(sx, sy + gap);
    path.line_to(sx + thickness * 0.5, sy + gap + thickness * 0.5);
    path.line_to(sx + thickness, sy + gap);
    path.line_to(sx + thickness, sy + sh - gap);
    path.line_to(sx + thickness * 0.5, sy + sh - gap - thickness * 0.5);
    path.line_to(sx, sy + sh - gap);
    path.close();
    ctx.fill_path(path);
  };

  // Segment a (top)
  if (segments[digit][0]) {
    drawHorizontalSegment(x, y);
  }

  // Segment b (top right)
  if (segments[digit][1]) {
    drawVerticalSegment(x + width - thickness, y, height * 0.5);
  }

  // Segment c (bottom right)
  if (segments[digit][2]) {
    drawVerticalSegment(x + width - thickness, y + height * 0.5, height * 0.5);
  }

  // Segment d (bottom)
  if (segments[digit][3]) {
    drawHorizontalSegment(x, y + height - thickness);
  }

  // Segment e (bottom left)
  if (segments[digit][4]) {
    drawVerticalSegment(x, y + height * 0.5, height * 0.5);
  }

  // Segment f (top left)
  if (segments[digit][5]) {
    drawVerticalSegment(x, y, height * 0.5);
  }

  // Segment g (center)
  if (segments[digit][6]) {
    drawHorizontalSegment(x, y + height * 0.5 - thickness * 0.5);
  }
}

void FakeVideoCapturer::DrawColon(BLContext& ctx,
                                  double x,
                                  double y,
                                  double height) {
  double dot_size = height * 0.1;
  ctx.fill_circle(x + dot_size, y + height * 0.3, dot_size);
  ctx.fill_circle(x + dot_size, y + height * 0.7, dot_size);
}

#endif  // USE_FAKE_CAPTURE_DEVICE
