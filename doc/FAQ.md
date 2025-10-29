# FAQ

## Can I use it commercially?

Momo is licensed under the [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0), allowing commercial use.

## Can I specify a codec?

You can specify a codec in Ayame mode and Sora mode.

## Can I specify a bitrate?

You can specify a bitrate only in Sora mode.

## Can I use simulcast?

Only available in Sora mode.

The video codecs that can be used with simulcast are VP8, VP9, ​​AV1, H.264, and H.265.
To use this codec, specify `--simulcast`.

## Can I use AV1?

AV1 can be used in all modes.

## Can I use H.266?

H.265 requires a hardware accelerator.

- On Linux and Windows, an NVIDIA GPU or Intel GPU (integrated is acceptable) is required.
- On macOS, use Apple Video Toolbox.

## Can I use the data channel?

The data channel is only available via serial in P2P and Ayame modes.

[USE_SERIAL.md](USE_SERIAL.md)

## Can I add a certificate authority certificate?

You can add a CA certificate to be used for server certificate verification by specifying the path to the CA certificate in the `SSL_CERT_DIR` or `SSL_CERT_FILE` environment variable.

```bash
export SSL_CERT_FILE=/path/to/cert.pem
./momo sora ...
```

## Can I ignore the server certificate?

You can disable server certificate verification on the client side by specifying the `--insecure` option.

```bash
./momo --insecure sora ...
```

## Can I use the NVIDIA Video Codec built into my NVIDIA video card?

It can be used on Windows and Ubuntu.
Please make sure your NVIDIA video card driver is up to date.

Check below for video cards that support NVENC.

[Video Encode and Decode GPU Support Matrix \| NVIDIA Developer](https://developer.nvidia.com/video-encode-decode-gpu-support-matrix#Encoder)

### Verified Video Cards

**Please contact us on the #momo-nvidia-video-codec-sdk Discord channel**

- GeForce GTX 1080 Ti
- @melpon
- GeForce GTX 1650
- @melpon
- GeForce GTX 1060 6GB
- @massie_g
- GeForce GTX 1050 Ti 4GB
- @cli_nil
- GeForce GTX 1070 with Max-Q Design 8GB
- @torikizi
- Quadro P1000 4GB
- OPTiM Corporation
- Quadro P4000
- OPTiM Corporation
- GeForce RTX 2070
- @msnoigrs
- GeForce RTX 2080
- @shirokunet
- GeForce RTX 3080
- @torikizi
- GeForce RTX 4060
- Shiguredo

## Do you have any recommendations for 4K cameras?

- Logitech MX Brio
- <https://www.logicool.co.jp/ja-jp/products/webcams/mx-brio.html>
- Insta360 Link 2 and Link 2C
- <https://www.insta360.com/jp/product/insta360-link2>

## Do you have any recommendations for 120fps cameras?

The Elgato FACECAM MK.2 currently supports 720p at 120fps.

<https://www.elgato.com/us/en/p/facecam-mk2>

## Can Momo input anything other than camera video?

Please refer to the following article.

[Streaming test video via WebRTC with a cameraless Raspberry Pi and Momo - Qiita](https://qiita.com/tetsu_koba/items/789a19cb575953f41a1a)

## Can Momo input anything other than microphone audio?

Please refer to the following article.

[Using an audio file instead of a microphone when transmitting via WebRTC with a Raspberry Pi and Momo - Qiita](https://qiita.com/tetsu_koba/items/b887c1a0be9f26b795f2)

## Can I use 60fps on macOS?

Momo runs at 60fps on macOS using H.264 or H.265 hardware acceleration.

## Can DataChannel messaging be used in Sora mode?

Momo does not plan to support Sora's DataChannel messaging. Support will be available in the Sora C++ SDK.

## Can I use the Raspberry Pi Camera?

Momo can use the Raspberry Pi Camera. It also supports libcamera-control, allowing you to set autofocus and other features.

For details, see [LIBCAMERA.md](LIBCAMERA.md).

## Does it support legacy versions of Raspberry Pi OS?

Momo does not support legacy versions and will only support the latest 64-bit versions.

## No video appears when `--hw-mjpeg-decoder true` is specified on Raspberry Pi (Raspberry-Pi-OS).

Raspberry Pi's MJPEG decoder only works with some MJPEG-compatible cameras.

Please use a CSI or USB camera that supports MJPEG, or set `--hw-mjpeg-decoder false`.

## How to stream H.264 FHD screen captures from macOS arm64

If you want to stream FHD screen captures from macOS arm64, set Sora's H.264 profile level ID to 3.2 or higher.

For configuration instructions, please refer to the [Sora documentation](https://sora-doc.shiguredo.jp/sora_conf#default-h264-profile-level-id).

If you do not change the profile level ID, please stream in H.264 HD or lower, or use another codec to stream FHD.

FHD streaming is not possible in P2P mode or Ayame mode, which do not allow you to change the profile level ID.

In these cases, please stream in H.264 HD or lower, or use another codec to stream FHD.

## Can I use H.264 on Windows?

H.264 can be used by using NVENC on NVIDIA video cards.
To check whether H.264 is available in your environment, use `./momo --video-codec-engines` and check the H264 option.

## Can I disable Sora's TURN function when using Sora mode?

Momo cannot be used with Sora's TURN function disabled.

## Can I disable the multi-stream function in Sora mode?

The multi-stream feature cannot be disabled in Momo's Sora mode.

## Can I configure a proxy?

You can use it by specifying the following options.

- `--proxy-url`: Proxy URL
- Example: <http://proxy.example.com:3128>
- `--proxy-username`: Username used for proxy authentication
- `--proxy-password`: Password used for proxy authentication

## Can I use H.264 on a 64-bit Raspberry Pi (Raspberry-Pi-OS)?

This is available from Release 2023.1.0 onwards.

## Can I transmit video at the resolution specified in the Momo options?

You can set the resolution using the `--resolution` option.

However, this is merely a request sent from Momo to the camera device, and the final resolution depends on the camera device's specifications.

If you would like to try a resolution not supported by your camera device, please refer to the following article.

[Sending at a resolution and frame rate not supported by the camera using WebRTC momo](https://zenn.dev/tetsu_koba/articles/c3b12bb5e52a57)

## I get an error when running Momo on Jetson. What is this?

When running Momo on Jetson, I get the error `Capture Plane: Error in VIDIOC_S_FMT: Invalid argument`.

This is because the program is creating potentially incompatible encoders and decoders to check for HWA compatibility.

This does not affect operation.

## I cannot receive AV1 video using SDL on Jetson.

Momo prioritizes hardware decoding, and only Jetson Orin supports AV1 hardware decoding.

If you are using a Jetson other than Jetson Orin, specify `software` with the `--av1-decoder` option.

## Can I use the Raspberry Pi Camera Module V3?

It is available from Release 2023.1.0 onwards.

## What version of JetPack is available?

It is only available with JetPack 5.1.1.

It is not available with versions other than JetPack 5.1.1.