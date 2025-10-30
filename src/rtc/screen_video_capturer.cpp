// The original is as follows:
// https://cs.chromium.org/chromium/src/content/browser/media/capture/desktop_capture_device.cc
//
// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in https://cs.chromium.org/chromium/src/LICENSE.

#include "screen_video_capturer.h"

#include <stdint.h>

#include <iostream>
#include <memory>

// WebRTC
#include <api/video/i420_buffer.h>
#include <modules/desktop_capture/cropped_desktop_frame.h>
#include <modules/desktop_capture/desktop_and_cursor_composer.h>
#include <modules/desktop_capture/desktop_capture_options.h>
#include <rtc_base/checks.h>
#include <rtc_base/logging.h>
#include <rtc_base/thread.h>
#include <rtc_base/time_utils.h>
#include <third_party/libyuv/include/libyuv.h>

#include "native_buffer.h"

const std::string ScreenVideoCapturer::GetSourceListString() {
  std::ostringstream oss;
  webrtc::DesktopCapturer::SourceList sources;
  if (GetSourceList(&sources)) {
    int i = 0;
    for (webrtc::DesktopCapturer::Source& source : sources) {
      // For the problem that the screen capture does not work on ubuntu,
      // The cause is not clear, but by inserting std::to_string,
      // the process that causes a segmentation violation can be avoided.
      // Whether the process that causes a segmentation violation can be avoided,
      // and in a clean installation environment, it works properly.
      oss << std::to_string(i++) << " : " << source.title << std::endl;
    }
  }
  return oss.str();
}

bool ScreenVideoCapturer::GetSourceList(
    webrtc::DesktopCapturer::SourceList* sources) {
  std::unique_ptr<webrtc::DesktopCapturer> screen_capturer(
      //webrtc::DesktopCapturer::CreateWindowCapturer(CreateDesktopCaptureOptions())
      webrtc::DesktopCapturer::CreateScreenCapturer(
          CreateDesktopCaptureOptions()));
  return screen_capturer->GetSourceList(sources);
}

ScreenVideoCapturer::ScreenVideoCapturer(
    webrtc::DesktopCapturer::SourceId source_id,
    size_t max_width,
    size_t max_height,
    size_t target_fps,
    bool include_cursor)
    : sora::ScalableVideoTrackSource(sora::ScalableVideoTrackSourceConfig()),
      max_width_(max_width),
      max_height_(max_height),
      requested_frame_duration_((int)(1000.0f / target_fps)),
      max_cpu_consumption_percentage_(50),
      quit_(false),
      include_cursor_(include_cursor) {
  auto options = CreateDesktopCaptureOptions();
  std::unique_ptr<webrtc::DesktopCapturer> screen_capturer(
      //webrtc::DesktopCapturer::CreateWindowCapturer(options));
      webrtc::DesktopCapturer::CreateScreenCapturer(options));
  if (screen_capturer && screen_capturer->SelectSource(source_id)) {
    if (include_cursor_) {
      capturer_.reset(new webrtc::DesktopAndCursorComposer(
          std::move(screen_capturer), options));
    } else {
      capturer_ = std::move(screen_capturer);
    }
  }

  capturer_->Start(this);
  capture_thread_ = webrtc::PlatformThread::SpawnJoinable(
      [this]() {
        while (CaptureProcess()) {
        }
      },
      "ScreenCaptureThread",
      webrtc::ThreadAttributes().SetPriority(webrtc::ThreadPriority::kHigh));
}

ScreenVideoCapturer::~ScreenVideoCapturer() {
  if (!capture_thread_.empty()) {
    quit_ = true;
    capture_thread_.Finalize();
  }
  output_frame_.reset();
  previous_frame_size_.set(0, 0);
  capturer_.reset();
}

webrtc::DesktopCaptureOptions
ScreenVideoCapturer::CreateDesktopCaptureOptions() {
  webrtc::DesktopCaptureOptions options =
      webrtc::DesktopCaptureOptions::CreateDefault();

#if defined(_WIN32)
  options.set_allow_directx_capturer(true);
#elif defined(__APPLE__)
  options.set_allow_iosurface(true);
#endif
  // set_mouse_cursor_shape_update_interval_ms is deprecated in WebRTC m138, so remove it

  return options;
}

void ScreenVideoCapturer::CaptureThread(void* obj) {
  // This function is no longer needed, but kept for compatibility (actually replaced with a lambda)
  auto self = static_cast<ScreenVideoCapturer*>(obj);
  while (self->CaptureProcess()) {
  }
}

bool ScreenVideoCapturer::CaptureProcess() {
  if (quit_) {
    return false;
  }

  int64_t started_time = webrtc::TimeMillis();
  capturer_->CaptureFrame();
  int last_capture_duration = (int)(webrtc::TimeMillis() - started_time);
  int capture_period =
      std::max((last_capture_duration * 100) / max_cpu_consumption_percentage_,
               requested_frame_duration_);
  int delta_time = capture_period - last_capture_duration;
  if (delta_time > 0) {
    webrtc::Thread::SleepMs(delta_time);
  }
  return true;
}

void ScreenVideoCapturer::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  //RTC_LOG(LS_ERROR) << __FUNCTION__ << " Start";
  bool success = result == webrtc::DesktopCapturer::Result::SUCCESS;

  if (!success) {
    //RTC_LOG(LS_ERROR) << __FUNCTION__ << " !success";
    return;
  }

  if (!previous_frame_size_.equals(frame->size())) {
    output_frame_.reset();
    capture_width_ = frame->size().width();
    capture_height_ = frame->size().height();
    if (capture_width_ > max_width_) {
      capture_width_ = max_width_;
      capture_height_ =
          frame->size().height() * max_width_ / frame->size().width();
    }
    if (capture_height_ > max_height_) {
      capture_width_ =
          frame->size().width() * max_height_ / frame->size().height();
      capture_height_ = max_height_;
    }
    //// std::cout << "capture_width_:" << capture_width_ << " capture_height_:" << capture_height_ << std::endl << std::flush;
    previous_frame_size_ = frame->size();
  }
  webrtc::DesktopSize output_size(capture_width_ & ~1, capture_height_ & ~1);
  if (output_size.is_empty()) {
    output_size.set(2, 2);
  }

  //RTC_LOG(LS_ERROR) << __FUNCTION__
  //  << " frame->size().width():" << frame->size().width()
  //  << " frame->size().height():" << frame->size().height()
  //  << " output_size.width():" << output_size.width()
  //  << " output_size.height():" << output_size.height();

  //webrtc::scoped_refptr<NativeBuffer> native_buffer(NativeBuffer::Create(
  //    webrtc::VideoType::kARGB, output_size.width(), output_size.height()));
  //native_buffer->InitializeData();
  webrtc::scoped_refptr<webrtc::I420Buffer> dst_buffer(
      webrtc::I420Buffer::Create(output_size.width(), output_size.height()));
  dst_buffer->InitializeData();

  if (frame->size().width() <= 2 || frame->size().height() <= 1) {
  } else {
    const int32_t frame_width = frame->size().width();
    const int32_t frame_height = frame->size().height();

    if (frame_width & 1 || frame_height & 1) {
      frame = webrtc::CreateCroppedDesktopFrame(
          std::move(frame),
          webrtc::DesktopRect::MakeWH(frame_width & ~1, frame_height & ~1));
    }
    const uint8_t* output_data = nullptr;
    int output_stride = 0;
    if (!frame->size().equals(output_size)) {
      if (!output_frame_) {
        output_frame_.reset(new webrtc::BasicDesktopFrame(output_size));
      }
      webrtc::DesktopRect output_rect;
      if ((float)output_size.width() / (float)output_size.height() <
          (float)frame->size().width() / (float)frame->size().height()) {
        int32_t output_height = frame->size().height() * output_size.width() /
                                frame->size().width();
        if (output_height > output_size.height())
          output_height = output_size.height();
        const int32_t margin_y = (output_size.height() - output_height) / 2;
        //RTC_LOG(LS_ERROR) << __FUNCTION__ << "output_size.width():" << output_size.width() << " output_height:" << output_height;
        output_rect = webrtc::DesktopRect::MakeLTRB(
            0, margin_y, output_size.width(), output_height + margin_y);
      } else {
        int32_t output_width = frame->size().width() * output_size.height() /
                               frame->size().height();
        if (output_width > output_size.width())
          output_width = output_size.width();
        const int32_t margin_x = (output_size.width() - output_width) / 2;
        //RTC_LOG(LS_ERROR) << __FUNCTION__ << "output_width:" << output_width << " output_size.height():" << output_size.height();
        output_rect = webrtc::DesktopRect::MakeLTRB(
            margin_x, 0, output_width + margin_x, output_size.height());
      }
      uint8_t* output_rect_data =
          output_frame_->GetFrameDataAtPos(output_rect.top_left());
      libyuv::ARGBScale(frame->data(), frame->stride(), frame->size().width(),
                        frame->size().height(), output_rect_data,
                        output_frame_->stride(), output_rect.width(),
                        output_rect.height(), libyuv::kFilterBox);
      output_data = output_frame_->data();
      output_stride = output_frame_->stride();
    } else {
      output_data = frame->data();
      output_stride = frame->stride();
      //RTC_LOG(LS_ERROR) << __FUNCTION__ << "output_stride:" << output_stride;
    }

    if (libyuv::ARGBToI420(
            output_data, output_stride, dst_buffer.get()->MutableDataY(),
            dst_buffer.get()->StrideY(), dst_buffer.get()->MutableDataU(),
            dst_buffer.get()->StrideU(), dst_buffer.get()->MutableDataV(),
            dst_buffer.get()->StrideV(), output_size.width(),
            output_size.height()) < 0) {
      RTC_LOG(LS_ERROR) << "ConvertToI420 Failed";
      return;
    }
    //for (uint32_t y = 0; y < output_size.height(); y++) {
    //  memcpy(native_buffer->MutableData() + y * native_buffer->width() * 4,
    //         output_data + y * output_stride, native_buffer->width() * 4);
    //}
  }

  webrtc::VideoFrame captureFrame = webrtc::VideoFrame::Builder()
                                        .set_video_frame_buffer(dst_buffer)
                                        .set_timestamp_rtp(0)
                                        .set_timestamp_ms(webrtc::TimeMillis())
                                        .set_rotation(webrtc::kVideoRotation_0)
                                        .build();
  ScalableVideoTrackSource::OnFrame(captureFrame);
}
