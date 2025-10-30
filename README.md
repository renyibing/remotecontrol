🌐 WebRTC Native Client Momo – Remote Control & Cloud Interaction

Next-Gen AI-Enabled Remote Interaction Platform
Built upon Shiguredo Momo
 and enhanced by Bob Haskins
Licensed under the Apache License 2.0

🚀 Project Vision

Momo Remote Control extends the original WebRTC Native Client Momo into a new-generation AI-ready remote computing platform that enables:

Ultra-low-latency remote desktops and cloud gaming

AI-driven workflow automation (connect n8n / Dify / AutoGen / Coze etc.)

Edge AI and Cloud Rendering for creative and industrial applications

Cross-platform interactive streaming for Windows, macOS, Linux, Android, iOS

This project explores the future of AI-powered remote collaboration, cloud computer rentals, and intelligent virtual desktops.

🙏 Acknowledgment

Special thanks to the Shiguredo Momo team for their pioneering open-source work on WebRTC Native Client Momo.
This fork continues their spirit of innovation while extending capabilities into interactive and intelligent remote-control applications.

🧠 AI & Monetization Ecosystem

The project is designed to evolve toward AI integrated remote computing and cloud service monetization:

💻 Cloud Desktop Rental (SaaS) – Provide on-demand GPU/CPU desktops for AI creators or gamers.

🤖 AI Agent Integration – Embed AutoGen / Coze / Dify agents directly into the remote environment for task automation.

🎮 Cloud Gaming Platform – Offer low-latency game streaming for PC and mobile devices.

💰 Usage-based Billing & Marketplace Models – Per-minute billing, template marketplaces, workflow subscriptions.

🧩 Web Embedding / SDK API – Developers can integrate the remote session engine into their AI apps or websites.

🔧 Core Technical Enhancements
🎹 Keyboard Hook Integration (New Feature)

Integrated libuiohook for intelligent keyboard interception and input mapping:

Automatic Focus Detection – Activates when the SDL window has mouse focus.

Automatic Release – Disengages on focus loss.

Cross-Platform Support – Windows / macOS / Linux

See 📄 doc/KEYBOARD_HOOK.md

🖥️ Architecture Overview

Sender → Captures screen + encodes audio/video → transmits via WebRTC

Receiver (SDL) → Decodes stream + controls remote input devices

Supported input devices:
✅ Keyboard ✅ Mouse (with cursor rendering) ✅ Gamepad / Controller ✅ Touch Input

🌍 Communication Modes
🟢 P2P Mode (LAN)

Direct peer-to-peer WebRTC connection within local networks

Ultra-low latency streaming and control

🔵 Relay Mode (via Ayame Signaling)

Secure relay communication over the internet

Automatic STUN / NAT traversal for seamless cross-network connectivity

Ideal for remote desktop access and AI-driven cloud workflow execution

![界面预览](./images/ea512f48-4b82-44c3-93db-a8572ddde107.png)

![界面预览](./images/f560ef54-f37b-4c91-b734-fb49ad9ad95c.png)


[![libwebrtc](https://img.shields.io/badge/libwebrtc-m138.7204-blue.svg)](https://chromium.googlesource.com/external/webrtc/+/branch-heads/7204)
[![GitHub tag (latest SemVer)](https://img.shields.io/github/tag/shiguredo/momo.svg)](https://github.com/shiguredo/momo)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Actions Status](https://github.com/shiguredo/momo/actions/workflows/build.yml/badge.svg)](https://github.com/shiguredo/momo/actions/workflows/build.yml)

## About Shiguredo's open source software

We will not respond to PRs or issues that have not been discussed on Discord. Also, Discord is only available in Japanese.

Please read <https://github.com/shiguredo/oss/blob/master/README.en.md> before use.

## About Shiguredo's Open Source Software

Please read <https://github.com/shiguredo/oss> before using.

## About WebRTC Native Client Momo

WebRTC Native Client Momo is a WebRTC native client that uses libwebrtc and operates in a variety of environments without a browser.

<https://momo.shiguredo.jp/>

### Hardware Accelerator Support

- You can use the hardware accelerator features built into the Raspberry Pi GPU via V4L2 M2M.
- Hardware Encoder: H.264
- Hardware Decoder: H.264
- The Raspberry Pi 5 does not support H.264 hardware acceleration because it does not have one.
- You can use the hardware accelerator features built into Apple macOS via Apple Video Toolbox.
- Hardware Encoder: H.264 / H.265
- Hardware Decoder: H.264 / H.265
- You can use the hardware accelerator features built into NVIDIA graphics cards via NVIDIA Video Codec. You can use the hardware accelerator functions built into Intel graphics chips via [Intel VPL](https://www.intel.com/content/www/us/en/developer/tools/vpl/overview.html) on Windows x86_64 and Ubuntu x86_64.
- Hardware Encoder: VP9 / AV1 / H.264 / H.265
- Hardware Decoder: VP9 / AV1 / H.264 / H.265
- Hardware Accelerator functions built into [NVIDIA Jetson](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/) are available via [Jetson JetPack](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/) on Windows x86_64 and Ubuntu x86_64. It can be used via the SDK.
- Hardware Encoder: VP9 / AV1 / H.264 / H.265
- Hardware Decoder: VP9 / AV1 / H.264 / H.265

### P2P Mode

Momo has its own signaling server, so it can be used in full P2P mode.

You can use it simply by accessing Momo from your browser.

### Ayame Mode

[OpenAyame](https://github.com/OpenAyame)

Momo has a mode that supports the Ayame WebRTC signaling server, and can also be used between Momos.

### Supports Raspberry Pi libcamera

Supports [raspberrypi/libcamera](https://github.com/raspberrypi/libcamera).

For details, see [LIBCAMERA.md](doc/LIBCAMERA.md).

### 120 fps Support

Momo supports uncompressed camera video, allowing 120 fps streaming by using a hardware encoder.

### 4K Streaming/Viewing

Momo supports 4K streaming/viewing via WebRTC by using a hardware accelerator.

### Simulcast Support

Momo supports simulcast (simultaneous streaming of multiple image qualities) when using Sora mode.

### Serial Reading/Writing via Data Channel

Momo supports direct serial reading and writing using the data channel. This is intended for use when low latency is prioritized over reliability.

### Receiving Audio and Video Using SDL

When using Momo in a GUI environment, you can receive audio and video using the [Simple DirectMedia Layer](https://www.libsdl.org/).

### H.265 (HEVC) Support

Support for sending and receiving H.265 using hardware acceleration is already available.

### AV1 Support

Support for sending and receiving AV1 is already available.

### YUY2 and NV12 Support

Momo supports uncompressed formats such as YUY2 and NV12.

### Client Certificate Support

Momo supports client certificates when using Sora mode.

### Using OpenH264

Momo can perform software encoding and decoding of H.264 using OpenH264.

## Video

[4K@30 Streaming with WebRTC Native Client Momo and Jetson Nano](https://www.youtube.com/watch?v=z05bWtsgDPY)

## About the OpenMomo Project

OpenMomo is a project that releases the WebRTC Native Client Momo as open source and continues to develop it.
We hope that it will enable the use of WebRTC for a variety of purposes beyond browsers and smartphones.

For more details, please see below.

[OpenMomo Project](https://gist.github.com/voluntas/51c67d0d8ce7af9f24655cee4d7dd253)

Tweets about Momo are also summarized below.

<https://gist.github.com/voluntas/51c67d0d8ce7af9f24655cee4d7dd253#twitter>

## Known Issues

[Resolution Policy for Known Issues](https://github.com/shiguredo/momo/issues/89)

## Binary Provision

You can download it from the link below.
<https://github.com/shiguredo/momo/releases>

## Operating environment

- Windows 11 x86_64
- macOS 15 arm64
- macOS 14 arm64
- Ubuntu 24.04 x86_64
- Ubuntu 22.04 x86_64
- Ubuntu 22.04 ARMv8 (NVIDIA Jetson JetPack 6) 
- [NVIDIA Jetson AGX Orin](https://www.nvidia.com/ja-jp/autonomous-machines/embedded-systems/jetson-orin/) 
- [NVIDIA Jetson Orin NX](https://www.nvidia.com/ja-jp/autonomous-machines/embedded-systems/jetson-orin/)
- Raspberry Pi OS bookworm (64bit) 
- Raspberry Pi 5 
- Raspberry Pi 4 
- Raspberry Pi 3
- Raspberry Pi 2 Model B v1.2
- Raspberry Pi Zero 2 W

## Getting Started

If you would like to use Momo, please read [USE.md](doc/USE.md).

## Building

- If you would like to build or package Momo, please read [BUILD.md](doc/BUILD.md).

## FAQ

Please read [FAQ.md](doc/FAQ.md).

## License

Apache License 2.0

```text
Copyright 2015-2025, tnoho (Original Author)
Copyright 2018-2025, Shiguredo Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at 

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expressed or implied.
See the License for the specific language governing permissions and
limitations under the License.
````

## OpenH264

<https://www.openh264.org/BINARY_LICENSE.txt>

```text
"OpenH264 Video Codec provided by Cisco Systems, Inc."
```

## Priority Implementation

Priority implementation refers to the early implementation of features planned for Momo, available exclusively to Sora licensees, for a fee.

- Windows version open source
- [Sloth Networks, Inc.](http://www.sloth-networks.co.jp)
- WebRTC Statistics support
- Company name not disclosed at this time
- Windows version of Momo with NVIDIA Video Codec support
- [Sloth Networks, Inc.](http://www.sloth-networks.co.jp)
- Linux version of Momo with NVIDIA Video Codec support
- [OPTiM Corporation](https://www.optim.co.jp/)
- Windows/Linux version screen capture support
- [Sloth Networks, Inc.](http://www.sloth-networks.co.jp)

### List of features available for priority implementation

**Features not listed here may be supported, so please contact us first.**

- Windows 11 arm64
- Ubuntu 20.04 arm64 (NVIDIA Jetson JetPack 5)

## E-book about Momo

Momo A book packed with Momo know-how, written by @tnoho, the original author of "Momo," is now on sale.

[Why not use WebRTC outside the browser to expand what you can do with it? \(Digital Edition\) \- DenDen Lab \- BOOTH](https://tnoho.booth.pm/items/1572872)

## Support

### Discord

- **No support**
- Advice provided
- Feedback welcome

The latest status updates are shared on Discord. Questions and inquiries are also accepted only through Discord.

<https://discord.gg/shiguredo>

### Bug Reports

Please report via Discord.

### Paid Technical Support

Paid technical support for WebRTC Native Client is available only to customers who have a WebRTC SFU Sora license agreement.

- Momo Technical Support
- Adding features to Momo with the intention of releasing them as open source software

## NVIDIA Video Codec

<https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/index.html>

```text
“This software contains source code provided by NVIDIA Corporation.”
```

## About H.264 License Fees

Distributing Momo by itself, which uses only the H.264 hardware encoder, does not require a license fee.
However, distributing it with hardware requires a license fee.

However, for Raspberry Pi, the H.264 license is included in the hardware cost, so no license fee is required.

For details, we recommend contacting [Via LA Licensing](https://www.via-la.com/).

We have contacted Via LA Licensing (formerly MPEG-LA) and confirmed that Momo's H.264 support is not subject to royalties.

> If Shiguredo provides a product that relies on an AVC/H.264 encoder/decoder already present on the end user's PC/device,
> the software product will not be subject to the AVC license and will not be eligible for royalties.

- The license fee for the Raspberry Pi hardware encoder is included in the price of the Raspberry Pi.
- <https://www.raspberrypi.org/forums/viewtopic.php?t=200855>
- Apple's license fee is limited to personal and non-commercial use; distribution requires a separate agreement with an organization.
- <https://store.apple.com/Catalog/Japan/Images/EA0270_QTMPEG2.html>
- The license fee for the AMD video card hardware encoder requires a separate agreement with an organization.
- <https://github.com/GPUOpen-LibrariesAndSDKs/AMF/blob/master/amf/doc/AMF_API_Reference.pdf>
- The license fee for the NVIDIA video card hardware encoder requires a separate agreement with an organization.
- <https://developer.download.nvidia.com/designworks/DesignWorks_SDKs_Samples_Tools_License_distrib_use_rights_2017_06_13.pdf>
- The license fee for the NVIDIA Jetson Nano hardware encoder requires a separate contract with your organization.
- [About the H.264/H.265 Hardware Encoder License for the NVIDIA Jetson Nano]
- The Intel Quick Sync Video hardware encoder license fee requires a separate agreement with an organization.
- [QuickSync \- H\.264 patent licensing fees \- Intel Community](https://community.intel.com/t5/Media-Intel-oneAPI-Video/QuickSync-H-264-patent-licensing-fees/td-p/921396)

## About H.265 License Fees

Distributing Momo using only the H.265 hardware encoder does not require a license fee.
However, distributing it with hardware requires a license fee.

We have contacted the following two organizations regarding Momo's H.265 support and have confirmed that distributing binaries that use only the H.265 hardware accelerator does not require a license.

We have also confirmed that releasing Momo, which supports H.265 and utilizes only the H.265 hardware accelerator, as open source and distributing pre-built binaries does not require a license.

- [Access Advance](https://accessadvance.com/ja/)
- [Via LA Licensing](https://www.via-la.com/)