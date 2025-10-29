# Building Momo

First, download the Momo repository.

```bash
git clone https://github.com/renyibing/remotecontrol.git
```

## Prerequisites (Windows)

To build Momo for Windows, you need the following OS and applications:

- The latest version of Windows
- [Visual Studio 2022](https://visualstudio.microsoft.com/ja/downloads/) (any edition)
- Install C++-related components. MSVC and MSBuild are especially required.
- [Python](https://www.python.org/downloads/)
- Install the latest version.

## Prerequisites (macOS)

To build Momo for macOS, you need the following OS and applications:

- The latest version of macOS
- Xcode

You must launch Xcode at least once and accept the license.

## Prerequisites (Ubuntu)

To build Momo for Ubuntu, Raspberry OS, or Jetson, you will need the following operating systems:

- Ubuntu 22.04 LTS

You will also need to install several additional packages, such as CUDA and Clang.

For details on the required packages, see [build.yml](../.github/workflows/build.yml).

## Build

You can generate Momo for each target using the command `python3 run.py build <target>`.

```bash
# This command only works on Windows.
# Also, run it from the Command Prompt or PowerShell.
# It does not work on shells such as Git Bash or Cygwin.
python3 run.py build windows_x86_64

# This command only works on macOS
python3 run.py build macos_arm64

# This command only works on Ubuntu
python3 run.py build raspberry-pi-os_armv8
python3 run.py build ubuntu-22.04_x86_64
python3 run.py build ubuntu-22.04_jetson
```

The generated Momo executable binary is located in the `_build/<target>/release/momo` directory.

## Creating a package

You can generate a package for each target by specifying the `--package` option during build.

``bash
# Change the `windows_x86_64` part to match the target you are building for.
python3 run.py build windows_x86_64 --package
```

The generated package is saved in the `_package/<target>/release` directory with a name similar to `momo-2023.1.0_raspberry-pi-os_armv8.tar.gz`.

## Configuration after unpacking the package

```console
$ tree
momo
├── LICENSE
├── NOTICE
├── html
│ ├── p2p.html
│ └── webrtc.js
└── momo
```