# Trying out Momo with the NVIDIA Jetson Series

If you are purchasing the Jetson Series, please refer to [BUY_JETSON.md](BUY_JETSON.md).

The NVIDIA Jetson Series is designed to use only JetPack 6.0.0.

## Known Issues

Currently, there is an issue with Jetpack 6 where H.264 cannot be transmitted when --hw-mjpeg-decoder is enabled.
To transmit H.264 on the Jetson 6, specify `--hw-mjpeg-decoder=false`.
For details, see https://github.com/shiguredo/momo/issues/355.

## Binaries for the Jetson Series are provided below.

Download the latest binary from <https://github.com/shiguredo/momo/releases>.

- `momo-<version>_ubuntu-22.04_armv8_jetson.tar.gz`
- Jetson AGX Orin or Jetson NX Orin
- Compatible with the latest version of JetPack 6

## Downloaded package, unzipped configuration

```console
$ tree
.
├── html
│   ├── p2p.html
│   └── webrtc.js
├── LICENSE
├── momo
└── NOTICE
```

## Try it out

For instructions on how to run it, first check [USE_P2P.md](USE_P2P.md).

## Specifying the video device

For information on specifying the video device, check [LINUX_VIDEO_DEVICE.md](LINUX_VIDEO_DEVICE.md).

## Additional options for Jetson

### --hw-mjpeg-decoder

`--hw-mjpeg-decoder` enables hardware video resizing and hardware decoding of MJPEG for USB cameras.
The default for the Jetson series is `--hw-mjpeg-decoder=true`. If you want to use a codec that does not support hardware decoding, specify `--hw-mjpeg-decoder=false`.

``bash
./momo --hw-mjpeg-decoder=true --no-audio-device p2p
```

## Steps to achieve 4K@30fps

### About the execution command

Try removing `--fixed-resolution`. Using the `--fixed-resolution` option at 4K tends to result in unstable rates.

### No Frame Rate

The most common reason is that you are using the camera in a dark location. The camera automatically extends the exposure time, lowering the frame rate. Increase the brightness of the room. Or, if possible, change the camera settings to prioritize frame rate.

### If you are using a recommended camera with an IMX317 camera:

> v4l2-ctl --set-ctrl=exposure_auto=1

Change the camera settings by running: The following settings are capable of outputting 4K 30fps.

```console
$ v4l2-ctl --list-ctrls
brightness 0x00980900 (int) : min=-64 max=64 step=1 default=0 value=0
contrast 0x00980901 (int) : min=0 max=95 step=1 default=1 value=1
saturation 0x00980902 (int) : min=0 max=100 step=1 default=60 value=60
hue 0x00980903 (int) : min=-2000 max=2000 step=1 default=0 value=0
white_balance_temperature_auto 0x0098090c (bool) : default=1 value=1
gamma 0x00980910 (int) : min=64 max=300 step=1 default=100 value=100 
gain 0x00980913 (int) : min=0 max=255 step=1 default=100 value=100 
power_line_frequency 0x00980918 (menu) : min=0 max=2 default=1 value=1 
white_balance_temperature 0x0098091a (int) : min=2800 max=6500 step=1 default=4600 value=4600 flags=inactive 
sharpness 0x0098091b (int) : min=0 max=7 step=1 default=0 value=0 
backlight_compensation 0x0098091c (int) : min=0 max=100 step=1 default=64 value=64 
exposure_auto 0x009a0901 (menu) : min=0 max=3 default=3 value=1
exposure_absolute 0x009a0902 (int) : min=1 max=10000 step=1 default=156 value=156
error 5 getting ext_ctrl Pan (Absolute)
error 5 getting ext_ctrl Tilt (Absolute)
focus_absolute 0x009a090a (int) : min=0 max=1023 step=1 default=0 value=0 flags=inactive
focus_auto 0x009a090c (bool) : default=1 value=1
error 5 getting ext_ctrl Zoom, Absolute
```

## 4K@30fps execution example

This section describes how to run 4K@30fps using Jetson AGX Orin.

### Preliminary Checks

Before executing the 4K@30fps command, make sure you're ready.

- All setup for using momo with Jetson AGX Orin is complete.
- A camera capable of 4K@30fps is installed.
- A Sora/Sora Labo account is available.

### Execution

Here, we're using [Sora Labo](https://sora-labo.shiguredo.jp/), which allows businesses and other organizations to test it for free by applying for its use.

For information on applying for and using Sora Labo, please refer to the [Sora Labo documentation](https://github.com/shiguredo/sora-labo-doc).

An example command is provided below.

```bash
$ ./momo --hw-mjpeg-decoder true --framerate 30 --resolution 4K --log-level 2 sora \
--signaling-urls \
wss://canary.sora-labo.shiguredo.app/signaling \
-channel-id shiguredo_0_sora \
-video true --audio true \
-video-codec-type VP8 --video-bit-rate 15000 \
-auto --role sendonly \
-metadata '{"access_token": "xyz"}'
```

The command example is structured as follows:

- ./momo ~ sora are parameters for momo.
- `--hw-mjpeg-decoder true` enables Hardware Acceleration.
- `--framerate 30` sets the frame rate to 30.
- `--resolution 4K` sets the resolution to 4K.
- `--log-level 2` sets the error and warning logs to be output.
- `sora` sets the Sora mode.
- The second line after `sora` contains parameters for connecting to Sora.
- `wss://canary.sora-labo.shiguredo.app/signaling` sets the signaling URL.
- `shiguredo_0_sora` sets the channel ID.
- `--video true` enables video transmission to Sora.
- `--audio true` enables audio transmission to Sora.
- `--video-codec-type VP8` sets the codec to VP8.
- `--video-bit-rate 15000` sets the video bitrate to 1.5Mbps.
- `--auto` enables automatic connection to Sora.
- `--role sendonly` sets the role for sending to send only.
- `--metadata '{"access_token": "xyz"}'` sets the Sora Labo access token in the metadata.

### Execution Results

To check the execution results, use `chrome://webrtc-internals` in Chrome.

Checking `chrome://webrtc-internals` confirms 30 fps at 4K (3840x2160), as shown below.

[![Image from Gyazo](https://i.gyazo.com/df47a19994982ed963e84d88a