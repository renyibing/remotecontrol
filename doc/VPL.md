# Using a Hardware Encoder/Decoder with VPL on Momo

VPL enables you to use the HWA functionality of Intel Quick Sync Video on Momo.

This document describes how to set up VPL.

## About Intel Media SDK

For more information about VPL, see the link below.

- List of supported codec and chipset combinations for decoders and encoders
- <https://github.com/intel/media-driver#decodingencoding-features>
- VPL GitHub
- <https://github.com/intel/libvpl>
- Official Intel documentation
- <https://intel.github.io/libvpl/latest/index.html>

## Supported Platforms

- Ubuntu 24.04 x86_64
- Ubuntu 22.04 x86_64
- Windows 11 x86_64

## How to Use on Windows 11

### Driver Installation

You can use VPL on Windows 11 by installing the driver from Intel's official website.

- Download the driver from Intel's official website.
- Download Intel drivers and software
- <https://www.intel.co.jp/content/www/jp/ja/download-center/home.html>
- Follow the instructions in the installer to install.
- Reboot after installation.

### Verify VPL is recognized

Running Momo with the `--video-codec-engines` option will output a list of available encoders and decoders. Codecs that display `Intel VPL [vpl]` in the `Encoder` and `Decoder` fields are supported.

Example PowerShell command:

```powershell
.\momo.exe --video-codec-engines
```

Example PowerShell execution result:

```powershell
> .\momo.exe --video-codec-engines
VP8:
Encoder:
- Software [software] (default)
Decoder:
- Software [software] (default)

VP9:
Encoder:
- Software [software] (default)
Decoder:
- Intel VPL [vpl] (default)
- Software [software]

AV1:
Encoder:
- Software [software] (default)
Decoder:
- Software [software] (default)

H264:
Encoder:
- Intel VPL [vpl] (default)
Decoder:
- Intel VPL [vpl] (default)
```

## How to use on Ubuntu

### Ubuntu How to use with 24.04

To install the runtime, you need to add the Intel apt repository.

bash
sudo apt update
sudo apt -y install wget gpg

# Install Intel's GPG key
wget -qO - https://repositories.intel.com/gpu/intel-graphics.key | sudo gpg --dearmor --output /usr/share/keyrings/intel-graphics.gpg
# Add Intel's repository
echo "deb [arch=amd64,i386 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/gpu/ubuntu noble client" | sudo tee /etc/apt/sources.list.d/intel-gpu-noble.list

sudo apt update
# Install libraries required for the Sora Python SDK
sudo apt -y install git libva2 libdrm2 make build-essential libx11-dev
# Intel VPL Install the required libraries
sudo apt -y install intel-media-va-driver-non-free libmfx1 libmfx-gen1 libvpl2 libvpl-tools libva-glx2 va-driver-all vainfo

# Check that vainfo can be run with sudo
sudo vainfo --display drm --device /dev/dri/renderD128

# Add a udev rule
sudo echo 'KERNEL=="render*" GROUP="render", MODE="0666"' > /etc/udev/rules.d/99-vpl.rules
# Reboot
sudo reboot

# Check that vainfo can be run without sudo
vainfo --display drm --device /dev/dri/renderD128
```

### How to use on Ubuntu 22.04

Use Intel's apt to install the runtime. You need to add a repository.

bash
sudo apt update
sudo apt -y install wget gpg

# Install Intel's GPG key
wget -qO - https://repositories.intel.com/gpu/intel-graphics.key | sudo gpg --dearmor --output /usr/share/keyrings/intel-graphics.gpg
# Add Intel's repository
echo "deb [arch=amd64,i386 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/gpu/ubuntu jammy client" | sudo tee /etc/apt/sources.list.d/intel-gpu-jammy.list

sudo apt update
# Install libraries required for the Sora Python SDK
sudo apt -y install git libva2 libdrm2 make build-essential libx11-dev
# Intel VPL Install the required libraries
sudo apt -y install intel-media-va-driver-non-free libmfx1 libmfx-gen1 libvpl2 libvpl-tools libva-glx2 va-driver-all vainfo

# Check that vainfo can be run with sudo
sudo vainfo --display drm --device /dev/dri/renderD128

# Add a udev rule
sudo echo 'KERNEL=="render*" GROUP="render", MODE="0666"' > /etc/udev/rules.d/99-vpl.rules
# Reboot
sudo reboot

# Check that vainfo can be run without sudo
vainfo --display drm --device /dev/dri/renderD128
```

### Check that VPL is recognized

Momo Running the command with the `--video-codec-engines` option will output a list of available encoders and decoders. Codecs that display `Intel VPL [vpl]` in `Encoder` and `Decoder` are available.

Example command:

```bash
./momo --video-codec-engines
```

Example execution result:

```bash
$ ./momo --video-codec-engines
VP8:
Encoder:
- Software [software] (default)
Decoder:
- Software [software] (default)

VP9:
Encoder:
- Software [software] (default)
Decoder:
- Intel VPL [vpl] (default)
- Software [software]

AV1:
Encoder:
- Software [software] (default)
Decoder:
- Software [software] (default)

H264:
Encoder:
- Intel VPL [vpl] (default)
Decoder:
- Intel VPL [vpl] (default)
```

## Verified Chipsets

The following chipsets are currently verified to work.

- Intel(R) Core(TM) Ultra 5 Processor 125H
- Intel(R) Core(TM) i9-9980HK
- Intel(R) Core(TM) i7-1195G7
- Intel(R) Core(TM) i5-10210U
- Intel(R) Processor N97
- Intel(R) Processor N100
- Intel(R) Processor N95

## Chipsets with Unconfirmed Operation

- Intel(R) Processor N150

## When Multiple Encoders Are Used

In an environment where NVIDIA is coexisting, both the INTEL and NVIDIA encoders are displayed.
Momo prioritizes NVIDIA, but you can use Intel VPL by specifying `vpl` with the `--h264-encoder` option.

## When VPL Is Not Recognized

VPL may not be recognized in environments using NVIDIA. In this case, you may be able to get it to recognize the device by removing the NVIDIA driver and switching to the Intel graphics driver.