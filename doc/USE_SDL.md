# Bidirectional SDL

**This feature is experimental**

## Overview

Using SDL (Simple DirectMedia Layer), Momo itself can output the video it receives.

## Notes

- This feature is only available in ayame and sora modes.
- In p2p mode, getUserMedia cannot be used because p2p.html is not HTTPS.
- This feature is available on Windows, macOS, or Linux.

## SDL Command Arguments

- --use-sdl
- Specify this if you want to use the SDL feature.
- --window-width
- Specify the width of the window in which the video will be displayed.
- --window-height
- Specify the height of the window in which the video will be displayed.
- --fullscreen
- Make the window in which the video will be displayed full-screen.

### Sora Mode

- --role sendonly, --sora recvonly, or --sora sendrecv
- Specify this if you want to switch roles in Sora. Specify sendonly for sending only, recvonly for receiving only, or sendrecv for both sending and receiving. sendrecv is only available for multi-stream. The default is sendonly.
- --spotlight
- Specify this when using the spotlight feature in Sora.

## 1:1 two-way communication using Ayame

- This is an example when not signing up for Ayame Labo.
- Please change the room ID to a value that is difficult to guess.
- This is an example when connecting two momos.

```bash
./momo --resolution VGA --no-audio-device --use-sdl ayame --signaling-url wss://ayame-labo.shiguredo.app/signaling --room-id momo-sdl-ayame
```

[![Image from Gyazo](https://i.gyazo.com/4fca01c1b92ff60f519b3e6c7941ed19.png)](https://gyazo.com/4fca01c1b92ff60f519b3e6c7941ed19)

## Multi-stream bidirectional communication using Sora

- The Signaling server URL is a dummy.

```bash
./momo --resolution VGA --no-audio-device --use-sdl sora --role sendrecv --video-codec-type VP8 --video-bit-rate 1000 --audio false --signaling-urls wss://example.com/signaling --channel-id momo-sdl-sora
```

[![Image from Gyazo](https://i.gyazo.com/e0c864b2e0a04fde210a2013ed634a53.png)](https://gyazo.com/e0c864b2e0a04fde210a2013ed634a53)

## Full screen

- Press f to go full screen, press f again to go back.
- Press q to Momo will shut down itself