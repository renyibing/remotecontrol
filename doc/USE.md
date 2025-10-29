# Getting Started with Momo

## Preparation

### Preparing Momo on the NVIDIA Jetson Series

Read [SETUP_JETSON.md](SETUP_JETSON.md).

### Preparing Momo on Raspberry Pi

Read [SETUP_RASPBERRY_PI.md](SETUP_RASPBERRY_PI.md).

### Preparing Momo on macOS

Read [SETUP_MAC.md](SETUP_MAC.md).

### Preparing Momo on Windows

Read [SETUP_WINDOWS.md](SETUP_WINDOWS.md).

### Preparing Momo on Ubuntu

Read [SETUP_UBUNTU.md](SETUP_UBUNTU.md).

## Getting it running

### Trying to run Momo using P2P mode

Please read [USE_P2P.md](USE_P2P.md).

### Trying to run Momo using Ayame mode

Ayame mode uses the open-source signaling server [WebRTC Signaling Server Ayame](https://github.com/OpenAyame/ayame) developed by Shiguredo.

By using [Ayame Labo](https://ayame-labo.shiguredo.app/), you can try Ayame without having to install it.

Please read [USE_AYAME.md](USE_AYAME.md).

### Trying to run Momo using Sora mode

Sora mode uses WebRTC SFU Sora, developed and sold by Shiguredo.

You can try Sora for free by using [Sora Labo](https://sora-labo.shiguredo.jp/).

Please read [USE_SORA.md](USE_SORA.md).

### Try serial reading and writing using the data channel

In P2P and Ayame modes, you can send and receive data to a specified serial port using the data channel.

Please read [USE_SERIAL.md](USE_SERIAL.md).

### Try using the receiving function using SDL

Momo can output audio and video using SDL (Simple DirectMedia Layer).

Please read [USE_SDL.md](USE_SDL.md).

### Obtaining Momo statistics

Momo provides a metrics API for viewing statistics.

Please read [USE_METRICS.md](USE_METRICS.md).

## FAQ

For the FAQ, please read [FAQ.md](FAQ.md).

## Commands

### Version Information

```
$ ./momo --version
WebRTC Native Client Momo 2022.2.0 (8b57be45)

WebRTC: Shiguredo-Build M102.5005@{#7} (102.5005.7.4 6ff73180)
Environment: [arm64] macOS Version 12.3 (Build 21E230)

- USE_JETSON_ENCODER
- USE_NVCODEC_ENCODER
```

### Help

````
$ ./momo --help
Momo - WebRTC Native Client
Usage: ./momo [OPTIONS] [SUBCOMMAND]

Options: 
-h,--help Print this help message and exit 
--help-all Print help message for all modes and exit 
--no-google-stun Do not use google stun 
--no-video-input-device Do not use video input device 
--no-audio-device Do not use audio device 
--force-i420 Prefer I420 format for video capture (only on supported devices) 
--hw-mjpeg-decoder BOOLEAN:value in {false->0,true->1} OR {0,1} 
Perform MJPEG deoode and video resize by hardware acceleration (only on supported devices) 
--use-libcamera Use libcamera for video capture (only on supported devices) 
--use-libcamera-native Use native buffer for H.264 encoding 
--video-input-device TEXT Use the video device specified by an index or a name (use the first one if not specified) 
--resolution TEXT Video resolution (one of QVGA, VGA, HD, FHD, 4K, or [WIDTH]x[HEIGHT]) 
--framerate INT:INT in [1 - 60] 
Video frame rate 
--fixed-resolution Maintain video resolution in degradation 
--priority TEXT:{BALANCE,FRAMERATE,RESOLUTION} 
Specifies the quality that is maintained against video degradation 
--use-sdl Show video using SDL (if SDL is available) 
--window-width INT:INT in [180 - 16384] 
Window width for videos (if SDL is available) 
--window-height INT:INT in [180 - 16384] 
Window height for videos (if SDL is available) 
--fullscreen Use fullscreen window for videos (if SDL is available) 
--version Show version information 
--insecure Allow insecure server connections when using SSL 
--log-level INT:value in {verbose->0,info->1,warning->2,error->3,none->4} OR {0,1,2,3,4} 
Log severity level threshold 
--screen-capture Capture screen 
--disable-echo-cancellation Disable echo cancellation for audio 
--disable-auto-gain-control Disable auto gain control for audio 
--disable-noise-suppression Disable noise suppression for audio 
--disable-highpass-filter Disable highpass filter for audio 
--video-codec-engines List available video encoders/decoders 
--vp8-encoder ENUM:value in {default->0,software->7} OR {0,7} 
VP8 Encoder 
--vp8-decoder ENUM:value in {default->0,software->7} OR {0,7} 
VP8 Decoder 
--vp9-encoder ENUM:value in {default->0,software->7} OR {0,7} 
VP9 Encoder 
--vp9-decoder ENUM:value in {default->0,software->7} OR {0,7} 
VP9 Decoder 
--av1-encoder ENUM:value in {default->0,software->7} OR {0,7} 
AV1 Encoder 
--av1-decoder ENUM:value in {default->0,software->7} OR {0,7} 
AV1 Decoder 
--h264-encoder ENUM:value in {default->0,videotoolbox->5} OR {0,5} 
H.264 Encoder 
--h264-decoder ENUM:value in {default->0,videotoolbox->5} OR {0,5} 
H.264 Decoder 
--serial TEXT:serial setting format 
Serial port settings for datachannel passthrough [DEVICE],[BAUDRATE] 
--metrics-port INT:INT in [-1 - 65535] 
Metrics server port number (default: -1) 
--metrics-allow-external-ip Allow access to Metrics server from external IP 
--client-cert TEXT:FILE Cert file path for client certification (PEM format) 
--client-key TEXT:FILE Private key file path for client certification (PEM format) 
--proxy-url TEXT Proxy URL 
--proxy-username TEXT Proxy username 
--proxy-password TEXT Proxy password

Subcommands: 
p2p P2P mode for momo development with simple HTTP server 
ayame Mode for working with WebRTC Signaling Server Ayame 
sora Mode for working with WebRTC SFU Sora
````

#### Video codec engine

````
$ ./momo --video-codec-engines
VP8: 
Encoder: 
-Software [software] (default) 
Decoder: 
-Software [software] (default)

VP9: 
Encoder: 
-Software [software] (default) 
Decoder: 
-Software [software] (default)

AV1: 
Encoder: 
-Software [software] (default) 
Decoder: 
-Software [software] (default)

H264: 
Encoder: 
- VideoToolbox [videotoolbox] (default) 
Decoder: 
- VideoToolbox [videotoolbox] (default)
````

### p2p mode help

````
$ ./momo p2p --help
P2P mode for momo development with simple HTTP server
Usage: ./momo p2p [OPTIONS]

Options: 
-h,--help Print this help message and exit 
--help-all Print help message for all modes and exit 
--document-root TEXT:DIR HTTP document root directory 
--port INT:INT in [0 - 65535]