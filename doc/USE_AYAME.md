# Try running Momo using Ayame mode

Ayame is a WebRTC signaling server developed by Shiguredo and released as open source.

[OpenAyame Project](https://gist.github.com/voluntas/90cc9686a11de2f1acca845c6278a824)

## Using Ayame Labo, a service using Ayame

Ayame Labo is provided for those who don't want to bother setting up a signaling server using Ayame.

Ayame Labo is a service using Ayame provided by Shiguredo. It is available for free.

<https://ayame-labo.shiguredo.app/>

### If you don't sign up for Ayame Labo

You can use Ayame Labo's signaling server without signing up.

Here, the room ID is set to `open-momo`, but be sure to change it to a value that is difficult to guess.

``bash
./momo --no-audio-device ayame --signaling-url wss://ayame-labo.shiguredo.app/signaling --room-id open-momo
```

#### For Windows

```powershell
.\momo.exe --no-audio-device ayame --signaling-url wss://ayame-labo.shiguredo.app/signaling --room-id open-momo
```

Since the Ayame SDK online sample cannot be used, please confirm the connection between Momo devices.

### If Signing Up for Ayame Labo

If you sign up for Ayame Labo, you must specify your GitHub username first in the room ID.
For example, if your GitHub username is `shiguredo`, the room ID will be `shiguredo@open-momo`.

- The room ID must start with your `GitHub username` + `@`.
- Here, the GitHub username is `shiguredo` and the room ID is `shiguredo@open-momo`.
- You must specify the signaling key with `--signaling-key`.
- Here, the signaling key is `xyz`.

```bash
./momo --no-audio-device ayame --signaling-url wss://ayame-labo.shiguredo.app/signaling --room-id shiguredo@open-momo --signaling-key xyz
```

#### For Windows

```powershell
.\momo.exe --no-audio-device ayame --signaling-url wss://ayame-labo.shiguredo.app/signaling --room-id shiguredo@open-momo --signaling-key xyz
```

Use the Ayame SDK online sample. Specify the room ID and signaling key as URL arguments to access.

<https://openayame.github.io/ayame-web-sdk/devtools/index.html?roomId=shiguredo@open-momo&signalingUrl=wss://ayame-labo.shiguredo.app/signaling&signalingKey=xyz>

## Controlling the Sending and Receiving Direction

In Ayame mode, you can use the `--direction` option to control the sending and receiving direction of video and audio.

### Available Values

- `sendrecv` - Send and receive both (default)
- `sendonly` - Send only
- `recvonly` - Receive only

### Sending Only

If you want to only send video and audio for streaming purposes, specify `--direction sendonly`.

```bash
./momo --no-audio-device ayame --signaling-url wss://ayame-labo.shiguredo.app/signaling --room-id open-momo --direction sendonly
```

### For receiving only

If you only want to receive video and audio, such as for viewing purposes, specify `--direction recvonly`.

```bash
./momo --no-audio-device ayame --signaling-url wss://ayame-labo.shiguredo.app/signaling --room-id open-momo --direction recvonly
```

### For both sending and receiving

For bidirectional communication, specify `--direction sendrecv` or omit the option.

```bash
./momo --no-audio-device ayame --signaling-url wss://ayame-labo.shiguredo.app/signaling --room-id open-momo --direction sendrecv
````