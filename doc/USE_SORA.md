# Running Momo with WebRTC SFU Sora

Sora is a commercial WebRTC SFU product developed and sold by Shiguredo.

<https://sora.shiguredo.jp/>

Here, we use [Sora Labo](https://sora-labo.shiguredo.app/), which can be tested for free by corporations and other organizations upon application.

For information on applying for and using Sora Labo, please refer to the [Sora Labo documentation](https://github.com/shiguredo/sora-labo-doc).

## Using Sora Labo

Create a GitHub account and sign up for <https://sora-labo.shiguredo.app/>.

### Try One-Way Broadcasting

- Specify `<github-username>_<github-id>_<your preferred string>` as the channel name.
- If your preferred string is "sora," your GitHub ID is "0," and your GitHub username is "shiguredo," specify `shiguredo_0_sora`.
- Here, the channel ID is `shiguredo_0_sora`.
- Specify the access token generated using the --metadata option in sora mode in `access_token`.
- Generate an access token in SoraLabo Home by entering the `<channel name>` you created earlier.
- This specification is not necessary if you are using the commercial version of Sora. This is a feature exclusive to Sora Labo.
- Here, the access token is `xyz`.

```bash
./momo --no-audio-device \
sora \
-signaling-urls \
wss://canary.sora-labo.shiguredo.app/signaling \
-channel-id shiguredo_0_sora \
-video-codec-type VP8 --video-bit-rate 500 \
-audio false \
-role sendonly --metadata '{"access_token": "xyz"}'
```

To check sending and receiving via a browser, please use the multi-stream receiving sample in Sora Labo.

### Try two-way streaming

Using Momo in a GUI environment allows you to receive audio and video using SDL.

```bash
./momo --resolution VGA --no-audio-device --use-sdl \
sora \
-signaling-urls \
wss://canary.sora-labo.shiguredo.app/signaling \
-channel-id shiguredo_0_sora \
-video-codec-type VP8 --video-bit-rate 1000 \
-audio false \
-role sendrecv --metadata '{"access_token": "xyz"}'
```

To check the multi-stream sending and receiving in the browser, please use the sample multi-stream sending and receiving in Sora Labo.

### Try Simulcast Broadcasting

```bash
./momo --no-audio-device \
sora \
-signaling-urls \
wss://canary.sora-labo.shiguredo.app/signaling \
-channel-id shiguredo_0_sora \
-video-codec-type VP8 --video-bit-rate 500 \
-audio false \
-simulcast true \
-role sendonly --metadata '{"access_token": "xyz"}'
```

To test sending and receiving in a browser, use the sample multi-stream simulcast reception in Sora Labo.