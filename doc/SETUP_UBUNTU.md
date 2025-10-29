# Try using Momo on Ubuntu 24.04 x86_64

## Binaries for Ubuntu 24.04 x86_64 are provided below.

Download the latest binary from <https://github.com/shiguredo/momo/releases>.

- For the binary, use `momo-<VERSION>_ubuntu-24.04_x86_64.tar.gz`.

## Downloaded package, configuration after extraction

```bash
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

### Package installation

Execute the following:

```bash
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install libdrm2 libva2 libva-drm2
```

If you want to use the Intel VPL, see [VPL.md](VPL.md).

## Granting Execution Permissions

Grant execution permissions to the downloaded momo executable file.

```bash
chmod a+x ./momo
```

## Running It

See [USE_P2P.md](USE_P2P.md) for instructions on how to run it.

## Specifying the Video Device

See [LINUX_VIDEO_DEVICE.md](LINUX_VIDEO_DEVICE.md) for instructions on how to specify the video device.