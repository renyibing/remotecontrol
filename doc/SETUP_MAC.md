# Trying out Momo on macOS

## Binaries for macOS are provided below.

Download the latest binary from <https://github.com/shiguredo/momo/releases>.

## Running it

See [USE_P2P.md](USE_P2P.md) for instructions on how to run it.

## --video-input-device

`--video-input-device` is a function that specifies the video device (i.e., camera) on macOS. This can be used when running multiple Momos on a single macOS and have multiple video devices, each of which you want to assign individually.

``bash
./momo --video-input-device 0 p2p
```

### Getting a list of devices

```bash
system_profiler SPCameraDataType
```

### How to specify a video device

You can specify a video device by device number or device name.

- If no device is specified, the device with device number 0 will be selected.
- `default` can be used as an alias for device number 0.
- If a device name is specified, the search will be performed sequentially, starting with device number 0, and the first matching device will be selected.

Note that device numbers vary depending on the connection order, so different devices may be selected even if the same device number is specified.
For this reason, if you want to fix the device to be used, specify it by device name.

#### Specifying the Default Device

The following all result in the same default device being selected.

```console
./momo p2p
./momo --video-input-device 0 p2p
./momo --video-input-device default p2p
./momo --video-input-device "default" p2p
```

#### Specify by device number

```console
./momo --video-input-device 1 p2p
```

#### Specify by device name

Since a prefix search is used to match, the same device will be selected for all of the following.

``console
./momo --video-input-device FaceTime p2p
./momo --video-input-device "FaceTime HD" 1 p2p
./momo --video-input-device "FaceTime HD Camera (Built-in)" p2p
```