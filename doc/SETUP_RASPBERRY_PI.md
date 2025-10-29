# Try using Momo on Raspberry Pi (Raspberry Pi OS)

## Note

Legacy versions of Raspberry Pi OS are not supported. Please use the latest version of Raspberry Pi OS (64-bit).

## Binaries for Raspberry Pi are provided below.

Download the latest binary from <https://github.com/shiguredo/momo/releases>.

- If you are using Raspberry Pi OS 64-bit, please use `momo-<VERSION>_raspberry-pi-os_armv8.tar.gz`.

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

## Preparation

### Package Installation

Please execute the following:

```bash
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install libnspr4 libnss3
sudo apt-get install libcamera0.5
```

#### Using Raspberry Pi OS Lite

Raspberry Pi Lite does not include video-related packages, so run `ldd ./momo | grep not` to check for missing packages.

An example of execution is shown below.

```bash
sudo apt-get install libxtst6
sudo apt-get install libegl1-mesa-dev
sudo apt-get install libgles2-mesa
```

#### Using a CSI camera such as the Raspberry Pi camera with Raspberry-Pi-OS

This option is unnecessary if using a USB camera.

Enable Camera in raspi-config.

Additionally, run the following command:

```bash
sudo modprobe bcm2835-v4l2 max_video_width=2592 max_video_height=1944
```

## Try it out

Please see [USE_P2P.md](USE_P2P.md).

## Specifying the video device

For specifying the video device, please see [LINUX_VIDEO_DEVICE.md](LINUX_VIDEO_DEVICE.md).

## Additional options for Raspberry Pi

### --force-i420

`--force-i420` forces capture to I420 instead of MJPEG, even at resolutions above HD, because using MJPEG for the Raspberry Pi dedicated camera reduces performance.
Do not use this option with USB cameras, as it will reduce the frame rate.

```bash
./momo --force-i420 --no-audio-device p2p
```

## Raspberry Pi dedicated camera cannot be used

Starting with Momo 2023.1.0, Raspberry Pi dedicated camera (CSI-connected camera) can be used only on Raspberry Pi OS (64-bit).

### --use-libcamera

`--use-libcamera` is the option to use the Raspberry Pi dedicated camera.

``bash
./momo --use-libcamera --no-audio-device p2p
```

## Poor performance with Raspberry Pi dedicated camera

### --hw-mjpeg-decoder

Consider using the MJPEG hardware decoder.
`--hw-mjpeg-decoder` enables hardware video resizing.

bash
./momo --hw-mjpeg-decoder true --no-audio-device p2p
```

### Review your Raspberry Pi settings

Please check [When using a CSI camera such as a Raspberry Pi camera with Raspberry Pi OS](#When using a CSI camera such as a Raspberry Pi camera with raspberry-pi-os).
In particular, if the `max_video_width=2592 max_video_height=1944` options are not specified, the frame rate will not be achieved at high resolutions.

### Review your options

When using a Raspberry Pi camera, using the `--hw-mjpeg-decoder=true --force-i420` options will reduce CPU usage and increase frame rate. For example, for the Raspberry Pi Zero, use the following command:

```bash
./momo --resolution=HD --force-i420 --hw-mjpeg-decoder=true p2p
```

This will set the maximum resolution in real time.

## Poor performance with USB camera

### --hw-mjpeg-decoder

When using certain MJPEG-compatible USB cameras, `--hw-mjpeg-decoder` will resize the video using hardware and decode MJPEG using hardware.

``bash
./momo --hw-mjpeg-decoder true --no-audio-device p2p
```

### Frame rate is not achieved when using a USB camera with a Raspberry Pi, even with --hw-mjpeg-decoder.

If you want to achieve frame rate when using a USB camera, we recommend not using `--hw-mjpeg-decoder`. However, this will increase CPU usage.

If you want to maintain a high frame rate while keeping CPU usage low, you can improve performance by adding the following to the end of `/boot/firmware/config.txt` and specifying `--hw-mjpeg-decoder`.

If you are using a version earlier than bookworm, add this to `/boot/config.txt`.

```text
gpu_mem=256
force_turbo=1
avoid_warnings=2
```

With these settings, you can achieve approximately 30 fps in HD and 15 fps in FHD.