# Changelog

- CHANGE
- Non-backward compatible changes
- UPDATE
- Backward compatible changes
- ADD
- Backward compatible additions
- FIX
- Bug fixes

## develop

- [CHANGE] Change the `--video-device` option to `--video-input-device`
- @voluntas
- [CHANGE] Change the `--no-video-device` option to `--no-video-input-device`
- @voluntas
- [UPDATE] Upgrade CUDA version to 12.9.1-1
- Add `D_ALLOW_UNSUPPORTED_LIBCPP` to the CUDA compile options
- Change the `cuda-gpu-arch` CUDA compile option from `sm_35` to `sm_60`
- sm_60 is a Pascal generation GPU Supported from
- sm_35 is supported from the Kepler generation GPU, but has been dropped because Kepler is only supported up to CUDA 10.
- sm_50 is supported from the Maxwell generation GPU, but has been dropped because Maxwell is only supported up to CUDA 11.
- @voluntas
- [UPDATE] Upgraded blend2d to version 0.20.0.
- Updated blend2d to reflect API changes: transitioned from camelCase to snake_case.
- Affected area: `src/rtc/fake_video_capturer.cpp` only.
- Examples of changes (old to new):
- `image_.getData(&data);` -> `image_.get_data(&data);`
- `ctx.setFillStyle(BLRgba32(0, 255, 255));` -> `ctx.set_fill_style(BLRgba32(0, 255, 255));`
- `path.moveTo(sx + gap, sy);` -> `path.move_to(sx + gap, sy);`
- Unchanged APIs
- `ctx.end()`, `ctx.save()`, and `ctx.restore()` are words, so they remain unchanged.
- @voluntas @torikizi
- [FIX] Fixed an issue where YUV was prioritized over MJPEG on cameras in Ubuntu environments.
- @melpon
- [FIX] Fixed an issue where `--video-codec-type` / `--audio-codec-type` were ignored due to case mismatch in Ayame mode.
- Changed the comparison between the specified codec name and the WebRTC-side `RtpCodecCapability::name` to be case-insensitive.
- Changed the auxiliary codec list to lowercase, and changed the comparison to ignore case when using `IsAuxiliaryCodec()`.
- Explicitly grouped primary and auxiliary codecs, ensuring the order passed to `SetCodecPreferences()`.
- @voluntas
- [FIX] Improved the Ayame client implementation.
- Fixed URL parsing to output the appropriate exception message.
- Added proper error handling when PeerConnection creation fails.
- Fixed asynchronous callbacks to use shared_from_this() appropriately.
- Replaced `boost::ignore_unused` with the C++17 `[[maybe_unused]]` attribute.
- Added detailed comments to the conditional expression in `should_create_answer`.
- Changed member variables to be initialized in the header file (`retry_count_`, `rtc_state_`, `is_send_offer_`, `has_is_exist_user_flag_`)
- Separate `ParseURL()`, `SetIceServersFromConfig()`, `CreatePeerConnection()`, and `SetCodecPreference()` from AyameClient and define them in an anonymous namespace.
- This makes it easier to understand what values ​​these functions depend on.
- `iceServers` is optional, so if it's not present, it will be ignored.
- @voluntas

### misc

- [ADD] Added E2E tests for the Raspberry Pi 64-bit environment
- Execute E2E tests for the Raspberry Pi OS armv8 environment using the self-hosted runner from GitHub Actions
- Added the test_sora_mode_raspberry_pi.py test file
- Added tests using camera capture using libcamera and the V4L2 M2M encoder
- @voluntas
- [FIX] Changed the build when using CUDA to use packages tailored for Ubuntu 22.04/24.04
- @voluntas

## 2025.1.0

**Release Date**: 2025-09-04

- [CHANGE] Changed the directory name of the release package to `momo-<version>_<platform>` when extracted
- @voluntas
- [CHANGE] Separated dependent library information from the VERSION file to the DEPS file
- VERSION The file now only lists the momo version number.
- Version information for dependent libraries is managed in a newly created DEPS file.
- @voluntas
- [CHANGE] Changed the run.py build command to the build subcommand.
- Changed the previous build using `python3 run.py <platform>` to `python3 run.py build <platform>`.
- @voluntas
- [CHANGE] Removed the multistream option from the Sora mode type: connect.
- Removed the multistream option because Sora has discontinued it.
- @voluntas
- [CHANGE] Migrated from SDL2 to SDL3.
- Upgraded SDL from 2.30.8 to 3.2.18.
- Made the following changes due to API changes in SDL3:
- Changed the header include from `<SDL.h>` to `<SDL3/SDL.h>`.
- `SDL_main.h` Added explicit include
- Changed `SDL_Init` return value check to boolean
- Removed `SDL_CreateWindow` positional argument
- Removed `SDL_RENDERER_ACCELERATED` flag
- Changed `SDL_SetWindowFullscreen` argument to boolean
- Changed `SDL_ShowCursor` / `SDL_HideCursor` to separate functions
- Changed event names to SDL3 format (e.g., `SDL_WINDOWEVENT` → `SDL_EVENT_WINDOW_RESIZED`)
- Changed key codes to uppercase (e.g., `SDLK_f` → `SDLK_F`)
- Changed `keysym.sym` to `key` in `SDL_KeyboardEvent`
- Changed `SDL_RenderCopy` to `SDL_RenderTexture`
- Changed `SDL_Rect` to `SDL_FRect` (to float coordinates)
- Changed `SDL_CreateRGBSurfaceFrom` to `SDL_CreateSurfaceFrom`
- Changed `SDL_FreeSurface` to `SDL_DestroySurface`
- Modified build settings:
- Windows: Disabled Game Input API and used static runtime libraries (/MT)
- Linux cross-compiling: Added `SDL_UNIX_CONSOLE_BUILD=ON` to skip X11/Wayland checks
- @voluntas
- [CHANGE] YUY2 now converts to NV12 in V4L2
- @melpon
- [CHANGE] Renamed test mode to p2p mode
- Changed command line option from `test` to `p2p`
- Changed HTML file name from `test.html` to Changed to `p2p.html`
- Changed the document file name from `USE_TEST.md` to `USE_P2P.md`
- @voluntas
- [UPDATE] Changed the blend2d download destination to the Shiguredo mirror and added a SHA256 hash check.
- Changed the download URL to <https://oss-mirrors.shiguredo.jp/>
- Implemented a SHA256 hash check function for downloads, similar to boost.
- @voluntas
- [UPDATE] Improved V4L2 video capture detection method.
- Changed to search for all video devices via `/sys/class/video4linux`.
- Added support for devices numbered `/dev/video64` and above.
- Filtered to only use devices that support the format.
- Added a numeric sorting process for devices.
- @voluntas
- [CHANGE] Changed the behavior of the `--force-i420` option.
- I420 If it is not available, the program will now stop with an error instead of falling back.
- @melpon @voluntas
- [UPDATE] Increased the maximum frame rate from 60 fps to 120 fps.
- Improved support for high frame rate cameras.
- @voluntas
- [UPDATE] Upgraded WebRTC to m138.7204.0.4.
- Added an include of `video_frame.h` to nvcodec_video_encoder.cpp with the update.
- Changed `frame.timestamp()` to `frame.rtp_timestamp()` in jetson_video_encoder, nvcodec_video_encoder, and vpl_video_encoder.
- Unified the `rtc::` and `cricket::` namespaces to the `webrtc::` namespace.
- Changed `absl::optional` to `std::optional`. to
- Added include for `<thread>` to main.cpp
- Changed `webrtc::SleepMs()` to `webrtc::Thread::SleepMs()`
- Changed `<rtc_base/third_party/base64/base64.h>` to `<rtc_base/base64.h>` and changed `webrtc::Base64::Encode()` to `webrtc::Base64Encode()`
- Added include for `<api/audio/builtin_audio_processing_builder.h>`, changed `dependencies.audio_processing` to `dependencies.audio_processing_builder`, and changed the creation method from `webrtc::AudioProcessingBuilder().Create()` to `webrtc::AudioProcessingBuilder().Create()` Changed to `std::make_unique<webrtc::BuiltinAudioProcessingBuilder>()`
- Removed `dependencies.task_queue_factory`, which is deprecated, and used `env` instead.
- Upgraded the clang version used from 18 to 20.
- Linked D3D11.lib on Windows.
- Used `webrtc::SdpVideoFormat::Parameters`, which is deprecated, instead used `webrtc::CodecParameterMap`.
- Used `webrtc::MediaType::MEDIA_TYPE_VIDEO`, which is deprecated, instead used `webrtc::MediaType::VIDEO`.
- @torikizi, @melpon
- [UPDATE] Upgraded CMake to 4.1.0
- @voluntas
- [UPDATE] Upgraded OpenH264 to 2.6.0
- @voluntas
- [UPDATE] Upgraded CLI11 to 2.5.0
- @voluntas
- [UPDATE] Upgraded Boost to 1.89.0
- Added SHA256 hash checking to buildbase.py
- @voluntas
- [UPDATE] Upgraded NVIDIA VIDEO CODEC SDK to 13.0
- @melpon
- [ADD] Added the `--fake-capture-device` option
- Added a fake video capture device using Blend2D
- Displayed a digital clock, animation, and 7-segment display
- Also added a fake audio device that generates a beep synchronized with the video
- Supported only on macOS and Ubuntu x86_64 platforms
- @voluntas
- [ADD] Added the format subcommand to run.py
- Executing clang-format with `python3 run.py format`
- @voluntas
- [ADD] Added `--disable-cuda` to the run.py arguments
- @melpon
- [ADD] Added libcamera control functionality
- Controls can be specified in key-value format with the `--libcamera-control` option
- Supports enum-type controls such as AfMode, AfRange, and AfSpeed
- Implemented an API for string-based control configuration
- @voluntas
- [ADD] Added the `--direction` option to Ayame mode
- Supports three send/receive directions: `sendrecv` (default), `sendonly`, and `recvonly`
- Controls the direction property of the WebRTC RTCRtpTransceiver
- Intended for use in streaming and viewing
- @voluntas
- [ADD] Added an option to specify the video and audio codec in ayame mode.
- The `--video-codec-type` option allows you to select from VP8, VP9, ​​AV1, H264, and H265.
- The `--audio-codec-type` option allows you to select from OPUS, PCMU, and PCMA.
- [ADD] Added a keyframe interval setting for the Intel VPL encoder.
- Set GopPicSize to 20 times the frame rate to generate keyframes every 20 seconds.
- Set IdrInterval to 0 to convert all I-frames to IDR frames.
- Example: At 120fps, a keyframe is generated every 2400 frames.
- @voluntas
- [ADD] Added the `--force-nv12` option.
- Force the use of the NV12 format in V4L2 capture.
- Stop with an error if NV12 is not available.
- The NV12 format is used as is without conversion.
- @voluntas
- [ADD] Added the `--force-yuy2` option.
- Forces the use of the YUY2 format in V4L2 capture.
- Stops with an error if YUY2 is not available.
- @melpon @voluntas
- [ADD] Added the `EnumV4L2CaptureDevices()` function to obtain a list of V4L2 video capture devices.
- Pass this list to `FormatV4L2Devices()` to create a printable string.
- @melpon
- [ADD] Added the `--list-devices` option in Linux environments.
- Ability to display a list of available video devices.
- Display grouped by camera.
- Detailed display of supported formats, resolutions, and frame rates for each device.
- @voluntas
- [FIX] Fixed SDL3 library version mismatch warning when building on macOS.
- Fixed CMAKE_OSX_DEPLOYMENT_TARGET when building SDL3. Set to the same value as WebRTC
- Resolved warnings caused by SDL3 being built on macOS 15.0 and Momo targeting macOS 12.0
- @voluntas
- [FIX] Fixed an issue where keyframe requests were not working in the VP9 encoder on Intel VPL.
- Fixed VP9 to only set `MFX_FRAMETYPE_I`.
- This is because setting `MFX_FRAMETYPE_REF` and `MFX_FRAMETYPE_IDR` at the same time would change them to `MFX_FRAMETYPE_P` by CheckAndFixCtrl in vpl-gpu-rt.
- @voluntas
- [FIX] Fixed an issue where the Dependency Descriptor RTP header extension was not output in the AV1 encoder on Intel VPL.
- Added the SVC controller for AV1, which was already implemented in AMD AMF and NVIDIA Video Codec SDK, to Intel VPL.
- This allows AV1 to be used in Intel VPL as well. RTP packets now include proper dependency information when encoding.
- @voluntas

### misc

- [ADD] Added an E2E test job using Intel VPL
- Added a mechanism to test the Intel VPL encoder using a self-hosted runner in GitHub Actions
- Added the test_sora_mode_intel_vpl.py test file
- @voluntas
- [UPDATE] Changed ubuntu-latest to ubuntu-22.04 in GitHub Actions
- Intentionally did not upgrade to ubuntu-24.04
- @voluntas
- [UPDATE] Updated actions/download-artifact and actions/checkout
- Updated from `@v4` to `@v5`
- @voluntas @torikizi

## 2024.1.4

**Release Date**: 2025-08-27

- [FIX] Version number correction error
- @voluntas

## 2024.1.3

**Release Date**: 2025-08-27

- [FIX] Removed processing of Sora's network.status notify event.
- Removed unstable_level due to the deprecation of Sora's network.status due to the new mechanism.
- @voluntas

## 2024.1.2

**Release Date**: 2025-06-10

- [FIX] Fixed an issue where libcamera 0.4 did not work in the latest Raspberry Pi OS environment.
- The momo 2024.1.1 release binary depends on libcamera.so 0.4, but it is incompatible with the latest Raspberry Pi OS environment. Therefore, it must be upgraded to libcamera.so 0.5.
- Rebuilding in the latest environment resolved the issue.
- @torikizi

## 2024.1.1

**Release Date**: 2025-02-26

**Due to a release error in the 2025-02-17 release, we have rebuilt and re-released the binaries.**

- [FIX] Fixed an issue where libcamera 0.3 did not work on the latest Raspberry Pi OS environment.
- The momo 2024.1.0 release binary depends on libcamera.so 0.3, but it is incompatible with the latest Raspberry Pi OS environment. Therefore, it must be upgraded to libcamera.so 0.4.
- Rebuilding in the latest environment resolved the issue.
- @melpon, @torikizi

### misc

- [FIX] Changed the Boost download URL.
- @torikizi
- [FIX] Fixed the installation of libssl1.1 for Ubuntu 22.04 and 24.04.
- Ubuntu 22.04 and 24.04 do not include libssl1.1 by default, so it must be installed.
- @torikizi

## 2024.1.0

**Release Date**: 2024-09-18

- [CHANGE] `--video-device` now specifies a device name like `MX Brio` instead of a file name like `/dev/video0`
- @melpon
- [CHANGE] Completely overhauled the build process
- @melpon
- [CHANGE] Removed raspberry-pi-os_armv6 and raspberry-pi-os_armv7
- @melpon
- [CHANGE] Removed ubuntu-20.04_x86_64
- @melpon
- [CHANGE] Removed ubuntu-20.04_armv8_jetson_xavier package
- NVIDIA JetPack SDK: JetPack 5.x is no longer supported
- @melpon
- [CHANGE] JetPack 5.1.2 Supports
- Tested with JetPack 5.1.1 and 5.1.2
- Tested that JetsonJpegDecoder fails due to compatibility issues in JetPack 5.1
- @enm10k
- [CHANGE] Changed `CreateVideoEncoder` and `CreateVideoDecoder` to `Create` due to changes in the inherited classes defined in libwebrtc.
- @melpon
- [CHANGE] Ported hwenc_nvcodec from the Sora C++ SDK.
- @melpon
- [UPDATE] Upgraded VPL to 2.13.0
- @voluntas
- [UPDATE] Upgraded CLI11 to 2.4.2
- @voluntas @torikizi
- [UPDATE] Upgraded SDL to 2.30.7
- @voluntas @torikizi
- [UPDATE] Upgraded Boost. Upgrade to 1.86.0
- @torikizi @voluntas
- [UPDATE] Upgrade WebRTC to m128.6613.2.0
- The changes in m128.6613.2.0 are as follows:
- Since helpers were removed from libwebrtc and split into `crypto_random`, `crypto_random.h` was added to use `rtc::CreateRandomString`.
- Reference: <https://source.chromium.org/chromium/_/webrtc/src/+/4158678b468135a017aa582f038731b5f7851c82>
- Modified to use `proxy_info_revive.h` and `crypt_string_revive.h`, which were removed from libwebrtc and resurrected in webrtc-build.
- Updated `init_allocator` arguments
- Removed `packetization_mode` and headers in accordance with the changes in the webrtc-build H.265 patch.
- Added the new ScreenCaptureKit framework to `CMakeLists.txt`, as it is required from m128 onwards.
- Reference: <https://source.chromium.org/chromium/_/webrtc/src/+/d4a6c3f76fc3b187115d1cd65f4d1fffd7bebb7c>
- @torikizi @melpon
- [UPDATE] Updated the related libraries required for WebRTC m119.
- Updated CMAKE_VERSION to 3.30.3.
- Updated CXX_STANDARD to the latest version to match the clang and CXX_STANDARD version upgrades.
- Changed CXX_STANDARD in set_target_properties on all platforms. Updated to 20
- Updated the clang version used on Ubuntu to 15
- @torikizi
- [UPDATE] Followed package directory changes
- The package directory changed when WebRTC was upgraded to m118, so this was updated accordingly
- @torikizi
- [UPDATE] Updated the Raspberry Pi OS build from bullseye to bookworm
- Changed the multistrap suite from bullseye to bookworm
- Fixed to install libstdc++-11-dev
- @torikizi
- [UPDATE] Fixed CMakeList.txt
- Since the CUDA version required by STL is 12.4 or higher, it was ignored to avoid impacting other platforms.
- Reference: <https://stackoverflow.com/questions/78515942/cuda-compatibility-with-visual-studio-2022-version-17-10>
- @torikizi
- [ADD] Added the ubuntu-22.04_armv8_jetson package.
- @melpon
- [ADD] Added support for the Intel VPL H.265 hardware encoder/decoder.
- @melpon
- [ADD] Added support for the NVIDIA Video Codec SDK H.265 hardware encoder/decoder.
- @melpon
- [ADD] Added support for the videoToolbox H.265 hardware encoder/decoder.
- @melpon
- [ADD] Added support for the Jetson H.265 hardware encoder/decoder.
- @melpon
- [ADD] Added support for the OpenH264 H.264 software encoder.
- @melpon
- [ADD] Added support for Ubuntu 24.04.
- @melpon
- [ADD] Added support for the Intel VPL AV1 hardware encoder.
- @tnoho
- [ADD] Added support for the Intel VPL VP9 hardware encoder.
- @tnoho
- [FIX] Fixed an issue where USB-connected cameras could no longer be acquired on macOS.
- Fixed an issue where USB devices could no longer be acquired on macOS.
- Since the previous API could no longer be used to acquire devices on macOS 14 and later, a new API has been created and is now used on macOS 14 and later.
- @torikizi

### misc

- [CHANGE] Changed SDL2 download location to GitHub
- @voluntas
- [UPDATE] Updated actions/download-artifact for Github Actions
- Updated for Node.js 16 deprecation
- Updated from actions/download-artifact@v3 to actions/download-artifact@v4
- @torikizi
- [UPDATE] Updated Windows for Github Actions to 2022
- @melpon

## 2023.1.0

- [CHANGE] Removed the --show-me option
- @melpon
- [CHANGE] Removed the ubuntu-18.04_armv8_jetson_nano and ubuntu-18.04_armv8_jetson_xavier packages
- @melpon
- [UPDATE] Updated CMake Upgraded to 3.27.6
- @voluntas
- [UPDATE] Upgraded SDL to 2.28.3
- @voluntas, @melpon
- [UPDATE] Upgraded CLI11 to 2.3.2
- @voluntas, @melpon
- [UPDATE] Upgraded Boost to 1.83.0
- @melpon @voluntas
- [UPDATE] Upgraded WebRTC to m117.5938.2.0
- Supported VP9/AV1 simulcasting
- @melpon, @torikizi
- [UPDATE] Upgraded NVIDIA VIDEO CODEC SDK to 12.0
- @melpon
- [UPDATE] Stopped using the deprecated actions/create-release and actions/upload-release and started using softprops/action-gh-release instead
- @melpon
- [UPDATE] Since `cricket::Codec` became protected in m116, we've fixed it to use `cricket::CreateVideoCodec`.
- @torikizi
- [UPDATE] Fixed VPL Init to be called every time.
- Fixed a crash issue with the Sora C++ SDK when receiving VP8 on some Windows systems. Deployed the fix to momo.
- @melpon
- [UPDATE] Changed the ADM used on Raspberry Pi 4 from ALSA to PulseAudio.
- @melpon
- [ADD] Added Ubuntu 22.04 x86_64.
- @melpon
- [ADD] Added ubuntu-20.04_armv8_jetson_xavier (a package compatible with JetPack 5.1.1).
- @melpon
- [ADD] Added support for the Raspberry Pi dedicated camera (libcamera).
- `--use-libcamera` and `--use-libcamera-native` options added.
- @melpon
- [ADD] Support for H.264 encoding and decoding using V4L2 m2m.
- @melpon
- [ADD] Enable SDL2 in Raspberry Pi armv8 builds.
- @melpon
- [FIX] Fixed an issue where an abnormal termination occurred when specifying a metadata value that could not be parsed into JSON.
- @miosakuma
- [FIX] Fixed an issue where stats could not be obtained when momo was offered in ayame mode.
- @kabosy620

## 2022.4.1

- [FIX] Removed "\r" characters from $GITHUB_OUTPUT on Windows in CI.
- @miosakuma

## 2022.4.0

- [CHANGE] `ubuntu-18.04_x86_64` Removed build of
- @miosakuma
- [CHANGE] Removed the `--multistream` option and fixed the value to true.
- @miosakuma
- [UPDATE] Upgraded Boost to 1.80.0.
- @melpon
- [UPDATE] Upgraded SDL to 2.24.1.
- @melpon
- [UPDATE] Upgraded cmake to 3.24.2.
- @voluntas
- [UPDATE] Upgraded libwebrtc to `M107.5304@{#4}`.
- @miosakuma
- @melpon
- [FIX] Fixed an issue where specifying 'none' for `--data-channel-signaling` and `--ignore-disconnect-websocket` would result in an error.
- @miosakuma
- [FIX] Changed the `--channel-id` option in ayame mode to `--room-id`. Fixed to
- @miosakuma

## 2022.3.0

- [CHANGE] Set `--multistream` to true by default
- @melpon
- [CHANGE] Removed `--role upstream` and `--role downstream`
- @melpon
- [CHANGE] Removed macos_x86_64 build
- @melpon
- [CHANGE] Removed the audio option --disable-residual-echo-detector
- @melpon
- [UPDATE] Upgraded `libwebrtc` to `M104.5112@{#8}`
- @voluntas, @melpon
- [ADD] Added HTTP proxy server settings for TURN-TLS
- `--proxy-url`
- `--proxy-username`
- `--proxy-password`
- @melpon
- [ADD] Added HTTP Proxy support for WebSockets for Sora signaling.
- @melpon
- [ADD] Added SNI support for the HTTP Proxy server.
- @melpon
- [FIX] Fixed a bug where multistream: true was always set regardless of the `--multistream` option in some Sora settings.
- @melpon

## 2022.2.0

- [UPDATE] Upgraded CLI11 to 2.2.0.
- @voluntas
- [UPDATE] Upgraded Boost to 1.79.0.
- @voluntas
- [UPDATE] Upgraded libwebrtc to M102.5005@{#1}.
- @tnoho @voluntas
- [ADD] Enable client authentication with `--client-cert` and `--client-key`.
- @melpon
- [ADD] NVIDIA VIDEO on Windows and Ubuntu Support for hardware decoders using CODEC SDK
- @melpon
- [ADD] Support for Intel Media SDK on Windows x86_64 and Ubuntu 20.04 x86_64
- @melpon
- [FIX] Fixed a crash when using Ubuntu 20.04 + H.264 + Simulcast + --hw-mjpeg-decoder true (#221)
- @melpon
- [FIX] Fixed an issue where Raspberry Pi + H.264 + --hw-mjpeg-decoder true did not work with some camera models (#141)
- @melpon
- [FIX] Fixed an issue where Raspberry Pi + H.264 + Simulcast + --hw-mjpeg-decoder true did not work (#236)
- @melpon
- [FIX] Using make_ref_counted with scoped_refptr on libwebrtc m100 Fixed the issue.
- @tnoho
- [FIX] SDL build fails on Mac due to declaration-after-statement errors. This has been patched to avoid this issue.
- @tnoho

## 2022.1.0

- [UPDATE] Supports Raspberry Pi OS bullseye.
- @tnoho
- [UPDATE] Upgraded JetPack to 4.6.
- @tnoho
- [UPDATE] Upgraded `libwebrtc` to `99.4844.1.0`.
- @tnoho
- [UPDATE] Upgraded sdl2 to 2.0.20.
- @voluntas
- [UPDATE] Upgraded cmake to 3.22.3.
- @voluntas
- [ADD] Changed DataChannel usage to create a DataChannel when making an Offer.
- @tnoho
- [FIX] Jetson Fixed a hardware decoder issue where the frame was not cropped to the output size during output.
- @tnoho
- [FIX] Fixed a problem where screen capture would crash on Linux.
- @tnoho

## 2021.6.0

- [UPDATE] Upgraded Boost to 1.78.0
- @voluntas
- [UPDATE] Upgraded cmake to 3.22.1
- @melpon
- [FIX] Fixed CaptureProcess termination. Changed the reference order of select's return value (retVal) and exit flag (quit\_).

- @KaitoYutaka

## 2021.5.0

- [UPDATE] Upgraded sdl2 to 2.0.18
- @voluntas
- [UPDATE] Upgraded cmake to 3.21.4
- @voluntas
- [UPDATE] Upgraded CLI11 to 2.1.2
- @voluntas
- [UPDATE] Upgraded `libwebrtc` to `97.4692.0.4`
- @melpon @voluntas
- [CHANGE] Requires the `--signaling-url` and `--channel-id` options to specify the signaling URL and channel ID.
- @melpon
- [UPDATE] Supports signaling mid.
- @melpon
- [ADD] Supports clustering by allowing multiple signaling URLs and supporting type: redirect.
- @melpon
- [FIX] Added `__config_site`, which is now required in libwebrtc m93.
- Ported from zakuro
- @melpon @voluntas
- [FIX] Fixed the issue where api/video-track_source_proxy.h was moved to pc/video_track_source_proxy.h in libwebrtc m93.
- Ported from zakuro
- @melpon @voluntas

## 2021.4.3

- [FIX] Fixed an issue where SSL connections with Let's Encrypt certificates failed.
- @melpon

## 2021.4.2

- [FIX] Changed the timing of SetParameters() to after SetLocalDescription() processing to allow Priority to function.
- @tsuyoshiii
- [FIX] Changed Priority from DegradationPreference align the conversion to .txt with actual behavior
- [UPDATE] Upgrade cmake to 3.21.3
- @voluntas

## 2021.4.1

- [FIX] Mimic Invoke-WebRequest as curl in the Windows release
- @melpon

## 2021.4

- [UPDATE] Upgrade Boost to 1.77.0
- @voluntas
- [UPDATE] Upgrade cmake to 3.21.2
- @voluntas
- [UPDATE] Upgrade `libwebrtc` to `M92.4515@{#9}`
- @melpon
- [UPDATE] Upgrade CLI11 to 2.0.0
- @melpon
- [UPDATE] Include AES-GCM as a candidate
- @melpon
- [ADD] DataChannel in Sora mode Supports signaling using .NET Framework
- Available from Sora 2021.1
- Added the following options:
- `--data-channel-signaling`
- `--data-channel-signaling-timeout`
- `--ignore-disconnect-websocket`
- `--disconnect-wait-timeout`
- @melpon
- [ADD] Supports signaling re-offer in Sora mode
- Available from Sora 2021.1
- @melpon
- [FIX] Supports simulcast active: false in Sora mode
- @melpon

## 2021.3

- [UPDATE] Upgraded `libwebrtc` to `M90.4430@{#3}`
- @voluntas
- [UPDATE] Upgraded Boost to 1.76.0
- @voluntas
- [UPDATE] Implemented cmake. Upgraded to 3.20.1
- @voluntas
- [FIX] Fixed outdated options in simulcast error messages.
- @enm10k

## 2021.2.3

- [UPDATE] Upgraded cmake to 3.20.0
- @melpon @voluntas
- [FIX] Fixed an issue where a segfault could occasionally occur depending on the initialization timing when specifying a HW encoder on the Jetson.
- @enm10k

## 2021.2.2

- [UPDATE] Upgraded cmake to 3.19.6
- @voluntas
- [UPDATE] Upgraded `libwebrtc` to `M89.4389@{#7}`
- @voluntas
- [FIX] Fixed an error message when running `momo --verson`
- Fixed an issue where an error was sent to standard output when checking whether a HW encoder was available and it was unavailable.
- @melpon @torikizi
- [FIX] Changed to using BoringSSLCertificate instead of OpenSSLCertificate.
- Fixed a segfault issue with TURN-TLS.
- @melpon @tnoho

## 2021.2.1

- [FIX] Fixed an issue where the Jetson would not work with libwebrtc built for ubuntu-18.04_armv8.
- @tnoho
- [UPDATE] Upgraded `libwebrtc` to `M89.4389@{#5}`
- @tnoho
- [UPDATE] Upgraded cmake to 3.19.5
- @voluntas

## 2021.2

- [CHANGE] Removed `NV12` from the supported pixel format list due to its unavailability in M89
- @tnoho
- [UPDATE] Upgraded `libwebrtc` to `M89.4389@{#4}`
- @tnoho
- [UPDATE] Upgraded JetPack to 4.5
- @tnoho
- [UPDATE] Upgraded cmake to 3.19.4
- @voluntas

## 2021.1

- [UPDATE] Upgraded cmake to 3.19.3
- @voluntas
- [UPDATE] Changed GitHub Actions' macOS build to macos-11.0
- @voluntas
- [UPDATE] Upgraded Boost to 1.75.0
- @voluntas
- [UPDATE] Changed nlohmann/json to Boost.JSON
- @melpon
- [ADD] Supported active and adaptivePtime for simulcast
- @melpon
- [ADD] Supported Apple Silicon
- @hakobera

## 2020.11

- [UPDATE] Upgraded cmake to 3.19.2
- @voluntas
- [UPDATE] Upgraded `libwebrtc` to `M88.4324@{#3}`
- @voluntas
- [ADD] Enable retrieving statistics via the HTTP API
- Added `--metrics-port INT` to specify the port number of the statistics server
- Loopback (listen on 127.0.0.1) is the default. To allow global access (listen on 0.0.0.0), specify the `--metrics-allow-external-ip` argument.
- @hakobera

## 2020.10

- [CHANGE] Change `--use-native` to `--hw-mjpeg-decoder=<bool>` to disable combination with software encoders.
- @melpon @tnoho
- [UPDATE] Upgrade `libwebrtc` to `M88.4324@{#0}`
- @tnoho @melpon @voluntas
- [UPDATE] Upgrade cmake to 3.18.4
- @voluntas
- [ADD] Enable VP8 HWA on Jetson Nano
- @tnoho

## 2020.9

- [CHANGE] Removed `ubuntu-16.04_x86_64_ros`
- @melpon
- [CHANGE] Changed Jetson frame conversion order
- @tnoho
- [CHANGE] Removed SDL from `raspberry-pi-os_armv8`
- @melpon
- [CHANGE] Requires true/false arguments for `--multistream` and `--simulcast`
- @melpon
- [CHANGE] Changed `--audio-bitrate` to `--audio-bit-rate`
- @melpon
- [CHANGE] Changed `--video-bitrate` to `--video-bit-rate`
- @melpon
- [CHANGE] Changed `--audio-codec` to `--audio-codec-type`
- @melpon
- [CHANGE] Changed `--video-codec` to `--video-codec-type`
- @melpon
- [CHANGE] Changed `--spotlight INT` to `--spotlight BOOL` (specify true/false)
- @melpon
- [UPDATE] Upgraded Boost to 1.74.0
- @voluntas
- [UPDATE] Upgraded cmake to 3.18.3
- @voluntas
- [UPDATE] Upgraded json to 3.9.1
- @voluntas
- [UPDATE] Upgraded `libwebrtc` to `M86.4240@{#10}`
- @voluntas
- [ADD] Added argument `--spotlight-number INT`
- @melpon
- [FIX] `SDL_PollEvent` Fixed the handling of .
- @melpon
- [FIX] Fixed the issue where `CoInitializeEx` was required before screen capture.
- @torikizi @melpon

## August 1, 2020

- [UPDATE] Upgraded `libwebrtc` to `M85.4183@{#2}`
- @voluntas

## August 2020

- [CHANGE] Changed package name `ubuntu-18.04_armv8_jetson` to `ubuntu-18.04_armv8_jetson_nano` and `ubuntu-18.04_armv8_jetson_xavier`
- @tnoho
- [ADD] Enable full-screen screen capture on macOS
- @hakobera
- [ADD] Enable VP9 HWA on Jetson Xavier series
- @tnoho @melpon
- [ADD] Added support for simulcast
- Available in Sora mode
- @melpon @shino
- [UPDATE] RootFS on Jetson Changed build method to obtain from repository.
- @tnoho
- [UPDATE] Upgraded `libwebrtc` to `M85.4183@{#1}`
- @hakobera @voluntas
- [UPDATE] Upgraded CLI11 to v1.9.1
- @voluntas
- [UPDATE] Upgraded json to 3.8.0
- @voluntas
- [UPDATE] Upgraded cmake to 3.18.0
- @voluntas

## 2020.7

- [UPDATE] Upgraded `libwebrtc` to `M84.4147@{#7}`
- @voluntas @melpon
- [UPDATE] Upgraded cmake to 3.17.3
- @voluntas
- [UPDATE] Upgraded Boost to 1.73.0
- @voluntas
- [UPDATE] Upgraded Jetson Nano libraries to NVIDIA L4T 32.4.2
- @melpon
- [ADD] Supported Ubuntu 20.04 x86_64
- @hakobera
- [ADD] Added `--video-codec-engines` option to display video encoder decoders.
- @melpon
- [ADD] Cached Boost for GitHub Actions.
- @melpon
- [ADD] Added full-screen screen capture functionality for Windows/Linux.
- Available with `--screen-capture` option.
- @melpon
- [ADD] Added `raspberry-pi-os_armv8`.
- @melpon
- [ADD] Implemented the ability to specify the video codec engine name.
- @melpon
- [CHANGE] Changed the package name `ubuntu-18.04_armv8_jetson_nano`. Changed to `ubuntu-18.04_armv8_jetson`
- @melpon
- [CHANGE] Changed package names `raspbian-buster_armv6` and `raspbian-buster_armv7` to `raspberry-pi-os_armv6` and `raspberry-pi-os_armv7`
- @melpon
- [FIX] Use dedicated functions for Windows ADM
- @torikizi @melpon
- [FIX] Corrected the help message for the --no-tty option in build.sh
- @hakobera

## 2020.6

- [UPDATE] Upgraded `libwebrtc` to `M84.4127@{#0}`
- @voluntas
- [ADD] Enable interconnection between Momo in test mode and Momo in Ayame mode
- @tnoho
- [CHANGE] Removed ubuntu-16.04_armv7_ros build
- @melpon

## 2020.5.2

- [FIX] Fixed AV1 not being available
- @torikizi @voluntas
- [UPDATE] Upgraded `libwebrtc` to `M84.4104@{#0}`
- @voluntas

## 2020.5.1

- [FIX] Fixed a typo in CMakeLists.txt
- @azamiya @torikizi @tnoho @melpon

## 2020.5

Release date: 2020.04.14

- [UPDATE] Upgraded `libwebrtc` to `M83.4103@{#2}`
- @voluntas
- [UPDATE] Upgraded `libwebrtc` to Upgraded to `M81.4044@{#13}`
- @voluntas
- [UPDATE] Upgraded `cmake` to `3.17.1`
- @voluntas
- [ADD] Experimental support for AV1
- Available only in Sora mode
- @voluntas @tnoho
- [FIX] Use PulseAudio instead of ALSA on Jetson Nano
- Fixed connection issues with Jetson Nano
- @azamiya @torikizi @tnoho @melpon

## 2020.4

Release Date: 2020.04.01

- [UPDATE] Upgraded `libwebrtc` to `M81.4044@{#11}`
- @voluntas
- [UPDATE] Upgraded `sdl2` to `2.0.12`
- @voluntas
- [UPDATE] Upgraded `cmake` to `3.17.0`
- @voluntas
- [ADD] Allow `--video-device` to be specified on Windows
- @msnoigrs
- [ADD] Added `--audio` and `--video` to the sora mode arguments
- @melpon
- [CHANGE] Removed the `--port` argument in root, and now specifies `--port` in `sora` and `test` modes
- @melpon
- [CHANGE] When `--port` is not specified in `sora` mode, automatically connect without specifying `--auto`.
- @melpon
- [CHANGE] Removed the `--daemon` argument.
- @melpon
- [CHANGE] Changed the `--no-video` and `--no-audio` arguments to `--no-video-device` and `--no-audio-device`.
- @melpon
- [CHANGE] Removed the PCMU audio codec.
- @melpon
- [CHANGE] When `--video-codec` or `--audio-codec` is not specified in sora mode, the default value for Sora will be used.
- Previously, it was VP8 and OPUS.
- @melpon
- [FIX] Removed the video*adapter* member variables, as they are not used.
- @msnoigrs
- [FIX] Ubuntu 18.04 Make it start even if `libcuda.so` / `libnvcuvid.so` are not installed.
- @melpon

## 2020.3.1

- [FIX] Enabling H.264 on ubuntu-18.04_x86_64
- @melpon

## 2020.3

- [UPDATE] Changing resizing to hardware when using H.264 on Raspberry Pi
- Changing from software-processed `vc.ril.resize` to hardware-processed `vc.ril.isp` in the VPU
- Changing conversion processing for different YUV formats to hardware
- @tnoho
- [UPDATE] Upgrading libwebrtc to M81.4044@{#9}
- @voluntas
- [UPDATE] Upgrading libwebrtc to M81.4044@{#7}
- @voluntas
- [UPDATE] Upgrading libwebrtc to M80.3987@{#6} Upgrading
- @voluntas
- [ADD] Support for H.264 hardware encoder using NVIDIA VIDEO CODEC SDK on Windows 10
- @melpon
- [ADD] Support for H.264 hardware encoder using NVIDIA VIDEO CODEC SDK on Ubuntu 18.04
- @melpon
- [ADD] Added the --insecure option to suppress TLS checks
- @melpon
- [ADD] Certificate checks for WSS and TURN-TLS now use both hardcoded and default paths in libwebrtc
- @melpon
- [ADD] Added a script for WebRTC customization
- @melpon
- [ADD] Acquire and send stats with `type: pong` when using Sora mode
- @melpon
- [ADD] Enable H264 hardware decoder when using SDL on Raspberry Pi
- @tnoho
- [FIX] Jetson Fixed a green band appearing at the bottom when using the --use-native option on the Nano with FHD settings.
- <https://github.com/shiguredo/momo/issues/124>
- @tetsu-koba @tnoho
- [FIX] Fixed a hang when stopping the H264 decoder on the Jetson Nano.
- @soudegesu @tnoho
- [FIX] Fixed an issue where the WebRTC version was not embedded on macOS.
- @melpon
- [FIX] Fixed an issue where the RTP timestamp was not set to 90kHz on the Jetson Nano.
- <https://github.com/shiguredo/momo/pull/137>
- @tetsu-koba @tnoho

## 2020.2.1

**Hotfix**

- [FIX] Fixed a crash when using the --use-sdl option on macOS.
- <https://bugzilla.libsdl.org/show_bug.cgi?id=4617>
- @melpon

## 2020.2

- [UPDATE] Update CLI11 to v1.9.0
- @voluntas
- [ADD] Add Windows 10 support
- @melpon
- [ADD] Add environment / libwebrtc / sora_client to signaling connection information when using Sora/Ayame mode on Windows
- `"environment": "[x64] Windows 10.0 Build 18362"`
- `"libwebrtc": "Shiguredo-Build M80.3987@{#2} (80.3987.2.1 fba51dc6)"`
- `"sora_client": "WebRTC Native Client Momo 2020.1 (0ff24ff3)"`
- @melpon
- [ADD] Changed build environment to CMake
- @melpon
- [CHANGE] Removed build of ubuntu-18.04_armv8
- @melpon

## 2020.1

- [UPDATE] Upgraded libwebrtc to M80.3987@{#2}
- libwebrtc hash is fba51dc69b97f6f170d9c325a38e05ddd69c8b28
- @melpon
- [UPDATE] Upgraded Momo to 2020.1
- Changed version number to <Release year>.<Number of releases in that year>
- @voluntas
- [UPDATE] Updated Boost to 1.72.0
- @voluntas
- [UPDATE] Added --video-device to Linux Enable it globally
- Changed to use V4L2 capturer
- @shino
- [UPDATE] Upgraded the Jetson Nano library to NVIDIA L4T 32.3.1
- [L4T \| NVIDIA Developer](https://developer.nvidia.com/embedded/linux-tegra)
- @melpon
- [UPDATE] Added the audio option --disable-residual-echo-detector
- @melpon
- [ADD] Added the ability to read and write to the serial port using the data channel
- Specifying --serial enables serial read and write via the data channel
- Available only in test and ayame modes
- @tnoho
- [ADD] Allows you to freely specify the resolution value
- You can now specify `--resolution 640x480`
- This feature is camera-dependent, so its functionality is not guaranteed.
- @melpon
- [ADD] Added the following to the signaling connection information when using Sora mode: Add environment/libwebrtc/sora_client
- For Jetson Nano
- `"environment": "[aarch64] Ubuntu 18.04.3 LTS (nvidia-l4t-core 32.2.1-20190812212815)"`
- `"libwebrtc": "Shiguredo-Build M80.3987@{#2} (80.3987.2.1 15b26e4)"`
- `"sora_client": "WebRTC Native Client Momo 2020.1 (f6b69e77)"`
- For macOS
- `"environment": "[x86_64] macOS Version 10.15.2 (Build 19C57)"`
- `"libwebrtc": "Shiguredo-Build M80.3987@{#2} (80.3987.2.1 15b26e4)"`
- `"sora_client": "WebRTC Native Client Momo 2020.1 (f6b69e77)"`
- Ubuntu 18.04 x86_64
- `"environment": "[x86_64] Ubuntu 18.04.3 LTS"`
- `"libwebrtc": "Shiguredo-Build M80.3987@{#2} (80.3987.2.1 15b26e4)"`
- `"sora_client": "WebRTC Native Client Momo 2020.1 (f6b69e77)"`
- @melpon
- [ADD] Add environ to signaling connection information when using Ayame mode. Add /libwebrtc/ayameClient
- Changes sora_client to ayameClient when using Sora
- @melpon
- [ADD] Adds a Raspbian mirror
- @melpon
- [CHANGE] Localizes momo --help to English
- @shino @msnoigrs
- [CHANGE] Removes <package>.edit functions and documentation
- @melpon
- [CHANGE] Disables SDL on armv6
- @melpon
- [FIX] Fixes the issue where the camera is briefly grabbed even when --no-video is specified
- @melpon @mganeko
- [FIX] Triggers an error when specifying SDL-related options when SDL is not enabled
- @melpon
- [FIX] Removes the requirement for Python 2.7 when building on macOS
- @melpon
- [FIX] Fixed an issue where the reconnection process would not proceed when a WebSocket was closed in Ayame mode
- @Hexa
- [FIX] Adding a close process when receiving a bye signal in Ayame mode.
- @Hexa
- [FIX] Changing the first reconnection process in Ayame mode to execute immediately after 5 seconds.
- @Hexa

## 19.12.1

- [UPDATE] Prevent premature build of libwebrtc
- Use <https://github.com/shiguredo-webrtc-build/webrtc-build>
- @melpon
- [FIX] Fixed the issue of delays when reconnecting in momo + ayame mode
- @kdxu

## 19.12.0

- [UPDATE] Changed the libwebrtc M79 commit position to 5
- The libwebrtc hash is b484ec0082948ae086c2ba4142b4d2bf8bc4dd4b
- @voluntas
- [UPDATE] Upgraded json to 3.7.3
- @voluntas
- [ADD] Allow sendrecv, sendonly, or recvonly to be specified for --role when using sora mode
- @melpon
- [FIX] QVGA Set the resolution to 320x240.
- @melpon @Bugfire
- [FIX] Fixed a segmentation fault that could occur when reconnecting in ayame mode.
- However, since "ping-pongs are not sent until both parties establish a connection" and "reconnection due to a ping timeout can take several seconds," the receiving party may have to wait several seconds after reconnection.
- This fix does not resolve the above issue.
- @kdxu
- [FIX] Do not explicitly build OpenH264.
- @melpon

## 19.11.1

- [ADD] Tested operation on Raspberry Pi 4
- @voluntas @Hexa
- [UPDATE] Changed libwebrtc M79 commit position to 3
- libwebrtc hash is 2958d0d691526c60f755eaa364abcdbcda6adc39
- @voluntas
- [UPDATE] Changed libwebrtc M79 commit position to 2
- libwebrtc hash is 8e36cc906e5e1c16486e60e62acbf79c1c691879
- @voluntas
- [UPDATE] Create two peer connections in Ayame if the isExistUser flag is not returned during accept
- [ADD] Enable audio and video reception using SDL `--use-sdl` Adding a new option
- [Simple DirectMedia Layer](https://www.libsdl.org/)
- @tnoho
- [ADD] Adding `--multistream` to SDL to support Sora's multistream
- @tnoho
- [ADD] Adding `--spotlight` to SDL to support Sora's spotlight
- @tnoho
- [ADD] Using the H.264 hardware decoder on the Jetson Nano when using SDL
- @tnoho
- [ADD] Adding `--show-me` to display your own camera video when using SDL
- @tnoho
- [ADD] Allowing you to specify the width of the video window when using SDL with `--window-width` and `--window-height`
- @tnoho
- [ADD] Adding `--fullscreen` to make the video window fullscreen when using SDL
- Pressing f will display full screen, and pressing f again will display full screen. Press to return.
- @tnoho
- [ADD] Allow `--role upstream` or `--role downstream` to be specified when using sora.
- @melpon
- [CHANGE] Changed the `isExistUser` flag returned by ayame's `accept` to determine whether to send an offer.
- @kdxu
- [FIX] Upgraded to C++14.
- @melpon
- [FIX] Fixed --video-codec to be usable even when USE_H264 is not defined.
- @msnoigrs

## 19.11.0

- [UPDATE] Upgraded json to 3.7.1.
- @voluntas
- [UPDATE] Changed the macOS build of GitHub Actions to macos-latest.
- @voluntas
- [UPDATE] Changed the libwebrtc M78 commit position to 8.
- libwebrtc The hash is 0b2302e5e0418b6716fbc0b3927874fd3a842caf
- @voluntas
- [ADD] Add ROS to GitHub Actions daily builds
- @voluntas
- [ADD] Add Jetson Nano and macOS to GitHub Actions builds
- @voluntas
- [ADD] Add documentation for 4K@30 output on Jetson Nano
- @tnoho @voluntas
- [ADD] Add --video-device option for macOS
- @hakobera
- [FIX] Fixed an error that occurred during GitHub Actions build due to insufficient disk space.
- @hakobera
- [FIX] Fixed random generation when ayame client ID was not specified.
- @kdxu
- [FIX] Fixed an issue where the ROS version was not building correctly.
- @melpon

## 19.09.2

- [UPDATE] Changed libwebrtc M78 commit position to 5
- libwebrtc hash is dfa0b46737036e347acbd3b47f0f58ff6c8350c8
- @voluntas
- [FIX] Set ice*servers* to only if iceServers is a json property and an array
- @kdxu

## 19.09.1

- [ADD] Implemented functionality to use the Jetson Nano hardware encoder
- @tnoho
- [ADD] Added Jetson Nano build
- @melpon
- [ADD] Switched CI from CircleCI to GitHub Actions
- Switched from weekly builds to daily builds only, since there is no time limit on macOS for OSS.
- @hakobera
- [ADD] .clang-format Added
- @melpon
- [UPDATE] Changed libwebrtc M78 commit position to 3
- libwebrtc hash is 68c715dc01cd8cd0ad2726453e7376b5f353fcd1
- @voluntas
- [UPDATE] Standardized command options as much as possible
- @melpon
- [UPDATE] Upgraded Raspberry Pi build OS from Ubuntu 16.04 to 18.04
- @melpon

## 19.09.0

- [ADD] Added the --disable-echo-cancellation option
- @melpon
- [ADD] Added the --disable-auto-gain-control option
- @melpon
- [ADD] Added the --disable-noise-suppression option
- @melpon
- [ADD] Added the --disable-highpass-filter option
- @melpon
- [ADD] Added the --disable-typing-detection option.
- @melpon
- [UPDATE] Updated to Boost 1.71.0.
- @voluntas
- [UPDATE] Changed the libwebrtc M78 commit position to 0.
- The libwebrtc hash is 5b728cca77c46ed47ae589acba676485a957070b.
- @tnoho
- [UPDATE] Changed the libwebrtc M77 commit position to 10.
- The libwebrtc hash is ad73985e75684cb4ac4dadb9d3d86ad0d66612a0.
- @voluntas
- [FIX] Fixed Track sharing across multiple PeerConnections.
- @tnoho
- [FIX] Fixed the issue where the capturer was checked even when the --no-audio setting was enabled.
- @tnoho
- [FIX] Fixed missing release of PeerConnectionObserver
- @tnoho

## 19.08.1

- [ADD] Added `--video-device` option for Raspberry Pi
- @melpon
- [UPDATE] Expose sora's metadata option
- @melpon

## 19.08.0

- [UPDATE] Update nlohmann/json to v3.7.0
- @melpon
- [UPDATE] Support Raspbian Buster
- @voluntas
- [UPDATE] Changed libwebrtc M77 commit position to 6
- libwebrtc hash is 71e2db7296a26c6d9b18269668d74b764a320680
- @voluntas
- [UPDATE] Changed libwebrtc M77 commit position to 3
- libwebrtc hash is 3d8e627cb5893714a66082544d562cbf4a561515
- @kdxu @voluntas
- [UPDATE] Changed libwebrtc M76 commit position to 3
- libwebrtc hash is 9863f3d246e2da7a2e1f42bbc5757f6af5ec5682
- @voluntas
- [UPDATE] Hardware resizing even for I420
- @tnoho
- [ADD] Added the --use-native option for Raspberry Pi
- Hardware decoding of MJPEG for USB cameras
- @tnoho
- [ADD] Added the --force-i420 option for Raspberry Pi
- Since MJPEG cannot be used for Raspberry Pi dedicated cameras, it forces I420 instead of MJPEG even for resolutions above HD. Capture with
- @tnoho
- [ADD] Add --signaling-key to Ayame subcommands
- @kdxu @tnoho
- [ADD] Support for iceServers distribution when using Ayame
- Custom STUN/TURN now available
- @kdxu @tnoho
- [CHANGE] Fix Ayame subcommands to allow the client ID to be specified as optional
- @kdxu
- [CHANGE] Change ./momo p2p to ./momo test
- @melpon
- [FIX] Fix an incorrect JSON schema for Ayame candidate exchange
- @kdxu
- [FIX] Fix the fixed "answer" type for Ayame SDP exchange
- @kdxu
- [FIX] Create an Ayame peer connection and then send it using createOffer I added a missing implementation.
- @kdxu
- [FIX] Fixed a bug that occurred when video could not be received after launching momo on Ayame.
- @kdxu
- [FIX] Fixed an issue where reconnection was sometimes impossible when using a hardware encoder on a Raspberry Pi.
- @tnoho
- [FIX] Fixed an issue where armv6 binaries created with libwebrtc M77 would crash.
- @tnoho
- [FIX] Fixed an issue where the macOS version of Momo would crash when using VideoToolbox.
- @hakobera
- [FIX] Fixed an issue where the macOS version would build successfully but result in a segmentation fault when trying to run it.
- @hakobera
- [FIX] Fixed an issue where the hardware encoder on a Raspberry Pi would consume GPU memory.
- @tnoho

## 19.07.0

- [UPDATE] MMAL support for H.264 on Raspberry Pi. Switch to hardware encoding using .NET Framework
- Enables 720p 30fps and 1080p 20fps
- @tnoho
- [UPDATE] Upgrade libwebrtc to M75
- libwebrtc hash is 159c16f3ceea1d02d08d51fc83d843019d773ec6
- @tnoho
- [UPDATE] Upgrade libwebrtc to M76
- libwebrtc hash is d91cdbd2dd2969889a1affce28c89b8c0f8bcdb7
- @kdxu
- [UPDATE] Supports Unified Plan
- @tnoho
- [UPDATE] Changed to disable AudioDevice when no-audio is enabled
- @tnoho
- [UPDATE] Upgrade CLI11 to v1.8.0 Update to
- @melpon
- [UPDATE] Update to JSON v3.6.1
- @melpon
- [UPDATE] Separate build documentation for macOS
- @voluntas
- [UPDATE] Delete doc/CACHE.md
- Build cache can now be deleted with make PACKAGE.clean
- @melpon
- [UPDATE] Move audio/video common options to sora options
- Codec and bitrate cannot be specified on Momo
- For P2P, switch SDP in HTML
- --audio-codec
- --audio-bitrate
- --video-codec
- --video-bitrate
- @melpon
- [UPDATE] Update to WebRTC Signaling Server Ayame 19.07.0
- @kdxu
- [ADD] Added support for WebRTC Signaling Server Ayame
- <https://github.com/OpenAyame/ayame>
- @kdxu
- [ADD] Automatically build the Linux version every day at 10:00 PM using Circle CI
- @voluntas
- [ADD] Automatically build the macOS version every Sunday at 10:00 PM using Circle CI
- @voluntas
- [FIX] Fixed an issue where devices could not be detected on macOS
- (Requires --fixed-resolution)
- @tnoho
- [FIX] Fixed an issue where ROS support could not be built
- @tnoho

## 19.02.0

- [UPDATE] Refactored webrtc.js / p2p.html
- [UPDATE] Modified webrtc.js to allow connections via WSS even when a reverse proxy or similar is installed in front of Momo and access is made via https.
- [CHANGE] Changed the build target, options, and package creation location.
- [UPDATE] Changed the STUN server URL specification from url to urls.
- [FIX] Fixed a segfault that occurred when launching in an environment without a camera.
- [FIX] Fixed the ARM ROS version to use a hardware encoder for H.264 streaming.
- [CHANGE] Added support for ROS Audio.
- Changed to use audio sent from another node.
- [UPDATE] Updated the libwebrtc library used to M73.

## 19.01.0

- [ADD] Ubuntu 16.04 ARMv7 ROS support

## 18.12.0

- [ADD] ROS support
- [CHANGE] Changed build target and package file name
- [CHANGE] Prevent automatic window closure after disconnecting from p2p mode
- [UPDATE] Updated to Boost 1.69.0
- [UPDATE] Updated to CLI11 v1.6.2
- [UPDATE] Updated to JSON v3.4.0
- [CHANGE] Disabled H264 for x86_64 and armv8

## 18.10.2

- [ADD] Added option to fix resolution
- [FIX] Fixed an issue where WebSocket frames that should have been separate were stuck together

## 18.10.1

- [UPDATE] Updated the libwebrtc library used to M71.
- [FIX] Make --metadata a Sora-only option.
- [FIX] Add --document-root to the P2P options.
- [FIX] Prevent the current directory from being exposed when the web server is started in P2P mode.
- [FIX] Fixed an issue where the bitrate specified in --auido-bitrate was treated as the video bitrate when --auido-bitrate was specified.

## 18.10.1-rc0

- [UPDATE] Replaced websocketpp and civietweb with Boost.beast.

## 18.10.0

**No changes from 18.10.0-rc4**

## 18.10.0-rc4

- [ADD] Supports 4K streaming.
- Supports armv6 and armv7, but currently only works with Raspberry Pi. Streaming is not possible due to insufficient machine power.

## 18.10.0-rc3

- [FIX] When version information is specified in MOMO_VERSION, the --version option of the momo binary is also reflected.
- [CHANGE] Only JSON can be specified as an argument to --metadata.

## 18.10.0-rc2

- [CHANGE] Removed 4K from the resolution specification because libwebrtc does not support 4K.
- Planned to support this in the future.
- <https://github.com/shiguredo/momo/issues/21>
- [FIX] Fixed an issue where audio did not play properly when video was enabled in the P2P mode sample.

## 18.10.0-rc1

- [UPDATE] Added a mechanism to compress binary size by passing `LDFLAGS += -s` during build.
- [ADD] Added licenses for libraries that WebRTC depends on.
- [ADD] Websocketpp Added licenses for libraries that depend on this feature.
- [FIX] Fixed a segfault that occurred when closing a window in P2P mode.

## 18.10.0-rc0

**First Release**

- Momo open-sourced under the Apache License 2.0
- libwebrtc M70 support
- Ubuntu 18.04 x86_64 support
- Binaries for Ubuntu 18.04 x86_64 provided
- Ubuntu 16.04 ARMv8 support
- Binaries for Ubuntu 16.04 ARMv8 provided
- PINE64 Rock64 support
- Raspberry Pi 3 B/B+ support
- ARMv7 binaries for Raspberry Pi 3 B/B+ provided
- H.264 HWA support for Raspberry Pi 3 B/B+ provided
- Raspberry Pi Zero W/WH support
- ARMv6 binaries for Raspberry Pi Zero W/WH provided
- H.264 HWA support for Raspberry Pi Zero W/WH provided
- Resolution Options
- QVGA, VGA, HD, FHD, 4K
- Frame Rate Options
- 1-60
- Priority Options
- This feature is experimental.
- You can specify whether to prioritize frame rate or resolution.
- Option to disable video
- Option to disable audio
- Video Codec Options
- Audio Codec Options
- Daemonization Options
- We recommend using Systemd.
- Log Level
- P2P Features
- WebRTC SFU Sora 18.04.12 Compatible