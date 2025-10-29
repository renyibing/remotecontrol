# Try using Momo on Windows

## Windows binaries are provided below.

Download the latest binary from <https://github.com/shiguredo/momo/releases>.

## Try running it

See [USE_P2P.md](USE_P2P.md) for instructions on how to run it.

### Notes when running in PowerShell or Command Prompt

When running in PowerShell or Command Prompt, due to string escaping specifications,
you must change "" to "\\\"" to enclose the metadata option key and value.

PowerShell execution example:

```bash
.\momo.exe --no-audio-device `
sora `
--signaling-urls `
wss://canary.sora-labo.shiguredo.app/signaling `
-channel-id shiguredo_0_sora `
-video-codec-type VP8 --video-bit-rate 500 `
-audio false `
-role sendonly --metadata '{\"access_token\": \"xyz\"}'
```