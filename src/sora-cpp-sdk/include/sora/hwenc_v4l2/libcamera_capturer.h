#ifndef SORA_HWENC_V4L2_LIBCAMERA_CAPTURER_H_
#define SORA_HWENC_V4L2_LIBCAMERA_CAPTURER_H_

#include <memory>
#include <mutex>
#include <queue>

#include "libcameracpp.h"

#include "sora/scalable_track_source.h"
#include "sora/v4l2/v4l2_video_capturer.h"

namespace sora {

struct LibcameraCapturerConfig : V4L2VideoCapturerConfig {
  LibcameraCapturerConfig() {}
  LibcameraCapturerConfig(const V4L2VideoCapturerConfig& config) {
    *static_cast<V4L2VideoCapturerConfig*>(this) = config;
  }
  LibcameraCapturerConfig(const LibcameraCapturerConfig& config) {
    *this = config;
  }
  // If native_frame_output == true, pass the captured data as a kNative frame.
  // If native_frame_output == false, copy the data and create an I420Buffer frame and pass it.
  // The former is more efficient, but the kNative frame does not automatically resize during simulcast, so it is better to use it according to the situation.
  bool native_frame_output = false;
  // libcamera control settings in key-value format.
  std::vector<std::pair<std::string, std::string>> controls;
};

// Class to capture video from a Raspberry Pi camera
// There are two output formats: fd-based and memory-based.
// The frame buffer passed is either a V4L2NativeBuffer class for fd-based output or a webrtc::I420Buffer class for memory-based output.
class LibcameraCapturer : public ScalableVideoTrackSource {
 public:
  static webrtc::scoped_refptr<LibcameraCapturer> Create(
      LibcameraCapturerConfig config);
  static void LogDeviceList();
  LibcameraCapturer();
  ~LibcameraCapturer();

  int32_t Init(int camera_id);
  void Release();
  int32_t StartCapture(LibcameraCapturerConfig config);

 private:
  static webrtc::scoped_refptr<LibcameraCapturer> Create(
      LibcameraCapturerConfig config,
      size_t capture_device_index);
  int32_t StopCapture();
  static void requestCompleteStatic(libcamerac_Request* request,
                                    void* user_data);
  void requestComplete(libcamerac_Request* request);
  void queueRequest(libcamerac_Request* request);

  std::shared_ptr<libcamerac_CameraManager> camera_manager_;
  std::shared_ptr<libcamerac_Camera> camera_;
  bool acquired_;
  std::shared_ptr<libcamerac_CameraConfiguration> configuration_;
  libcamerac_Stream* stream_;
  std::shared_ptr<libcamerac_FrameBufferAllocator> allocator_ = nullptr;
  struct Span {
    uint8_t* buffer;
    int length;
    int fd;
  };
  std::map<const libcamerac_FrameBuffer*, std::vector<Span>> mapped_buffers_;
  std::queue<libcamerac_FrameBuffer*> frame_buffer_;
  std::vector<std::shared_ptr<libcamerac_Request>> requests_;
  std::shared_ptr<libcamerac_ControlList> controls_;
  bool camera_started_;
  std::mutex camera_stop_mutex_;
};

}  // namespace sora

#endif  // LIBCAMERA_CAPTURER_H_