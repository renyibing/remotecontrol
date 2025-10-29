# Video Device Access in Linux

## Overview

Accessing video devices (cameras) using momo in a Linux environment requires appropriate user permissions.

momo selects cameras by specifying the device name, not the device file.

## Setting User Permissions

In Linux, group permissions are required to access video devices.

### Required Groups

- `video`: Required for accessing video devices (such as `/dev/video*`)

### Joining the Group

```bash
sudo usermod -a -G video $USER
```

### Checking Permissions

Check device file permissions:

```bash
ls -l /dev/video*
```

Typical Output:

```console
crw-rw---- 1 root video 81, 0 Oct 15 10:00 /dev/video0
crw-rw---- 1 root video 81, 1 Oct 15 10:00 /dev/video1
```

In this example, only users in the `video` group can read and write.

### Checking the current group

```bash
groups
```

### Applying group permissions

After joining a group, you must apply the permissions using one of the following methods:

1. Log out and log back in (recommended)
2. Start a new shell session:

``bash
newgrp video
``

3. Temporarily run with group permissions:

``bash
sg video -c 'command'
``

## Checking the video device list

### Checking devices with momo

```bash
./_build/ubuntu-24.04_x86_64/release/momo/momo --list-devices
```

Example output when working properly:

```
=== Available video devices ===

[/dev/video0] Insta360 Link: Insta360 Link (usb-0000:05:00.4-2.4):
Supported formats:
[0] MJPG (Motion-JPEG) 
1920x1080 (30 fps, 25 fps, 24 fps) 
1920x1440 (30 fps, 25 fps, 24 fps) 
1280x720 (30 fps, 25 fps, 24 fps) 
1280x960 (30 fps, 25 fps, 24 fps) 
3840x2160 (30 fps, 25 fps, 24 fps) 
[1] H264 (H.264) 
3840x2160 (30 fps, 25 fps, 24 fps) 
1920x1080 (30 fps, 25 fps, 24 fps) 
1920x1440 (30 fps, 25 fps, 24 fps) 
1280x720 (30 fps, 25 fps, 24 fps) 
1280x960 (30 fps, 25 fps, 24 fps) 

[/dev/video2] Logitech StreamCam (usb-0000:05:00.3-1): 
Supported formats: 
[0] YUYV (YUYV 4:2:2) 
640x480 (30 fps, 24 fps, 20 fps, 15 fps, 10 fps, 15/2 fps, 5 fps) 
176x144 (30 fps, 24 fps, 20 fps, 15 fps, 10 fps, 15/2 fps, 5 fps) 
320x240 (30 fps, (24 fps, 20 fps, 15 fps, 10 fps, 15/2 fps, 5 fps)
...
```

### Checking devices with v4l2-ctl

```bash
v4l2-ctl --list-devices
```

### Video devices recognized by the kernel

```bash
ls /dev/video*
```

## Selecting a video device

### How to specify a device

You can specify a video device:

```bash
./_build/ubuntu-24.04_x86_64/release/momo/momo --video-input-device "Logitech StreamCam" p2p
```

You can specify a device in the following ways:

- Device name: `--video-input-device "Logitech StreamCam"`
- Bus info: `--video-input-device "usb-0000:05:00.3-1"

### If multiple identical camera devices are connected

If multiple identical camera devices are connected, they will all have the same device name and cannot be specified individually.
In this case, you can specify `Bus info` instead of specifying the device name.

You can specify the `Bus info` (the `usb-...` part in parentheses) displayed by the `--list-devices` option.

```bash
./_build/ubuntu-24.04_x86_64/release/momo/momo --video-input-device "usb-0000:05:00.4-2.4" p2p
```

## Troubleshooting

### Video device not displayed

**Cause and Solution**

1. The user is not a member of the `video` group

```bash
sudo usermod -a -G video $USER
# Log out and log back in, or newgrp video
```

2. The camera is not connected or recognized

```bash
ls /dev/video*
v4l2-ctl --list-devices
```

3. Kernel driver issue

```bash
dmesg | grep -i video
```

### Check device file permissions

``bash
ls -l /dev/video*
```

## References

- Video4Linux (V4L2): <https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html>