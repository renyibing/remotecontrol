# Configuring Momo with libcamera

## Overview

Momo supports camera input using libcamera on Raspberry Pi OS 64-bit environments.
libcamera is a new camera stack for Linux that provides more advanced camera control than the previous V4L2.

## Basic Usage

### Enabling libcamera

```bash
./momo --use-libcamera sora ...
```

### Using Native Buffers

An optimization to reduce memory copies is available only when encoding H.264:

```bash
./momo --use-libcamera --use-libcamera-native sora ...
```

> [!NOTE]
> Restrictions:

- `--use-libcamera-native` is only available when encoding H.264 and simulcast is disabled.
- Best results are achieved when used in conjunction with a hardware encoder.

## Configuring libcamera Control

libcamera provides over 75 camera parameters. These can be configured in Momo using the `--libcamera-control` option.

### Basic Format

```bash
--libcamera-control <key> <value>
```

To set multiple controls:

```bash
./momo --use-libcamera \
-libcamera-control ExposureTime 10000 \
-libcamera-control AnalogueGain 2.0 \
-libcamera-control AfMode Continuous \
sora ...
```

### Supported Value Formats

#### Basic Types

- bool: `0` or `1`, `true` or `false`

``bash
-libcamera-control AeEnable 1
-libcamera-control AeEnable true
``

- int32: Integer Value

``bash
-libcamera-control ExposureTime 10000
``

- int64: Long Integer Value

``bash
--libcamera-control FrameDuration 33333
```

- float: Decimal value

```bash
--libcamera-control AnalogueGain 2.0
--libcamera-control Brightness -0.5
```

- enum: String or number (only major enums support strings)

```bash
--libcamera-control AfMode Continuous
# or
--libcamera-control AfMode 2
```

Enums that support strings:
- `AfMode`, `AfRange`, `AfSpeed`
- `AeMeteringMode`
- `AwbMode`
- `ExposureTimeMode`, `AnalogueGainMode`

#### Complex type

- Array: Comma-separated

```bash
--libcamera-control ColourGains 1.5,2.0
--libcamera-control FrameDurationLimits 33333,33333


Supported: `float[]`, `int32[]`, `int64[]`

- Rectangle: x,y,width,height

```bash
--libcamera-control ScalerCrop 100,100,640,480
```

- Multiple rectangles: Semicolon separated
```bash
--libcamera-control AfWindows "100,100,200,200;300,300,200,200"
```

> [!NOTE]
> For Rectangle arrays, separate multiple rectangles with semicolons.

- Matrix: Comma-separated (row-major) (unsupported)

``bash
--libcamera-control ColourCorrectionMatrix 1.0,0,0,0,1.0,0,0,0,1.0
```

### Unsupported types

- Size type
- Point type
- Matrix type (e.g., 3x3)
- Other complex structures

## Main controls

### Exposure control (AE: Auto Exposure)

| Control | Type | Description | Example values ​​|
|------------|---|------|-------|
| AeEnable | bool | Enable/Disable auto exposure | 0, 1 |
| ExposureTime | int32 | Exposure time (microseconds) | 10000 (1/100th of a second) |
| AnalogueGain | float | Analog gain (1.0 or higher) | 2.0 |
| ExposureTimeMode | enum | Exposure time mode | Auto, Manual |
| AnalogueGainMode | enum | Gain mode | Auto, Manual |
| AeMeteringMode | enum | Metering mode | CenterWeighted, Spot, Matrix |
| ExposureValue | float | EV compensation value | -2.0 to 2.0 |

### Autofocus (AF)

| Control | Type | Description | Example values ​​|
|------------|---|------|-------|
| AfMode | enum | AF mode | Manual, Auto, Continuous |
| AfRange | enum | Focus range | Normal, Macro, Full |
| AfSpeed ​​| enum | Focus speed | Normal, Fast |
| AfTrigger | enum | AF trigger | Start, Cancel |
| AfWindows | Rectangle[] | AF area | "256,192,512,384" |
| LensPosition | float | Lens position (diopters) | 2.0 |

### White balance (AWB)

| Control | Type | Description | Example values ​​|
|------------|---|------|-------|
| AwbEnable | bool | AWB enable/disable | 0, 1 |
| AwbMode | enum | AWB mode | Auto, Daylight, Cloudy, Tungsten |
| ColourTemperature | int32 | Color temperature (Kelvin) | 5500 |
| ColourGains | float[2] | Red and blue gains | 1.5, 2.0 |

### Image Quality Adjustment

| Control | Type | Description | Example Values ​​|
|------------|---|------|-------|
| Brightness | float | Brightness | -1.0 to 1.0 |
| Contrast | float | Contrast | 1.0 (Standard) |
| Saturation | float | Saturation | 1.0 (Standard), 0.0 (Monochrome) |
| Sharpness | float | Sharpness | 0.0 to 10.0 |
| Gamma | float | Gamma Value | 2.2 (Standard) |

### Frame Rate Control

| Control | Type | Description | Example Values ​​|
|------------|---|------|-------|
| FrameDurationLimits | int64[2] | Minimum/Maximum Frame Duration (microseconds) | 33333, 33333 (Fixed at 30 fps) |

## Enum Value List

### AfMode (Autofocus Mode)

- `Manual` or `0`: Manual focus
- `Auto` or `1`: Single AF (focuses once and stops)
- `Continuous` or `2`: Continuous AF

### AfRange (Focus Range)

- `Normal` or `0`: Normal range
- `Macro` or `1`: Macro (close-up)
- `Full` or `2`: Full range

### AfSpeed ​​(Focus Speed)

- `Normal` or `0`: Normal speed
- `Fast` or `1`: Fast speed

### ExposureTimeMode / AnalogueGainMode

- `Auto` or `0`: Automatic
- `Manual` or `1`: Manual

### AeMeteringMode (Metering Mode)

- `CentreWeighted` or `0`: Center-weighted metering
- `Spot` or `1`: Spot metering
- `Matrix` or `2`: Matrix metering

### AwbMode (White Balance Mode)

- `Auto` or `0`: Automatic
- `Incandescent` or `1`: Incandescent
- `Tungsten` or `2`: Tungsten
- `Fluorescent` or `3`: Fluorescent
- `Indoor` or `4`: Indoor
- `Daylight` or `5`: Daylight
- `Cloudy` or `6`: Cloudy

### HdrMode (HDR Mode)

- `Off` or `0`: Disabled
- `MultiExposureUnmerged` or `1`: Multiple exposure (unmerged)
- `MultiExposure` or `2`: Multiple exposure (merged)
- `SingleExposure` or `3`: Single exposure HDR
- `Night` or `4`: Night mode

## Usage Examples

### Example 1: Bright outdoor shooting settings

```bash
./momo --use-libcamera \
       --libcamera-control AeEnable 1 \
       --libcamera-control AeExposureMode Short \
       --libcamera-control AwbMode Daylight \
       --libcamera-control Contrast 1.2 \
       sora ...
```
### Example 2: Low-light shooting settings

```bash
./momo --use-libcamera \
--libcamera-control ExposureTimeMode Manual \
--libcamera-control ExposureTime 50000 \
--libcamera-control AnalogueGain 8.0 \
--libcamera-control NoiseReductionMode HighQuality \
sora ...
```

### Example 3: Macro shooting settings

```bash
./momo --use-libcamera \
--libcamera-control AfMode Continuous \
--libcamera-control AfRange Macro \
--libcamera-control AfSpeed ​​Normal \
--libcamera-control Sharpness 2.0 \
sora ...
```

### Example 4: Fixed frame rate (30 fps)

```bash
./momo --use-libcamera \
--libcamera-control FrameDurationLimits 33333,33333 \
sora ...
```

### Example 5: Full control with manual settings

```bash
./momo --use-libcamera \
--libcamera-control AeEnable 0 \
--libcamera-control AwbEnable 0 \
--libcamera-control ExposureTime 20000 \
--libcamera-control AnalogueGain 4.0 \
--libcamera-control ColourGains 1.8,1.5 \
--libcamera-control AfMode Manual \
--libcamera-control LensPosition 2.0 \
sora ...
```

## Troubleshooting

### Warning: "Unknown control"

The specified control name is invalid or the camera does not support that control.
Available controls vary depending on the camera model.

> [!NOTE]
> Although a warning is displayed, other valid settings are applied and the camera starts normally.

### Warning: "Unsupported control type for"

The specified control type is not supported (Size, Point, Matrix, etc.).

> [!NOTE]
> A warning will be displayed, but other valid settings will be applied and the camera will launch normally.

### Warning: "Invalid control value for"

The value format is incorrect or out of range.

- Check the numeric range.
- For enums, check valid values.
- For arrays, check the correct number of elements.

> [!NOTE]
> A warning will be displayed, but other valid settings will be applied and the camera will launch normally.

### Performance Issues

- Consider using the `--use-libcamera-native` option (H.264 only).
- Avoid unnecessary control settings.
- Set the frame rate limit appropriately.

## Control Key List

### Core Controls

```text
AeConstraintMode
AeEnable
AeExposureMode
AeFlickerDetected
AeFlickerMode
AeFlickerPeriod
AeMeteringMode
AeState
AfMetering
AfMode
AfPause
AfPauseState
AfRange
AfSpeed
AfState
AfTrigger
AfWindows
AnalogueGain
AnalogueGainMode
AwbEnable
AwbLocked
AwbMode
Brightness
ColourCorrectionMatrix
ColourGains
ColourTemperature
Contrast
DebugMetadataEnable
DigitalGain
ExposureTime
ExposureTimeMode
ExposureValue
FocusFoM
FrameDuration
FrameDurationLimits
FrameWallClock
Gamma
HdrChannel
HdrMode
LensPosition
Lux
Saturation
ScalerCrop
SensorBlackLevels
SensorTemperature
SensorTimestamp
Sharpness
```

### Draft Controls

```text
AePrecaptureTrigger
AwbState
ColorCorrectionAberrationMode
FaceDetectFaceIds
FaceDetectFaceLandmarks
FaceDetectFaceRectangles
FaceDetectFaceScores
FaceDetectMode
LensShadingMapMode
MaxLatency
NoiseReductionMode
PipelineDepth
SensorRollingShutterSkew
TestPatternMode
```

### Raspberry Pi Specific Controls

```text
Bcm2835StatsOutput
CnnEnableInputTensor
CnnInputTensor
CnnInputTensorInfo
CnnKpiInfo
CnnOutputTensor
CnnOutputTensorInfo
PispStatsOutput
ScalerCrops
StatsOutputEnable
SyncFrames
SyncMode
SyncReady
SyncTimer
```

## References

- <https://libcamera.org/api-html/namespacelibcamera_1_1controls.html>
- <https://github.com/raspberrypi/libcamera/blob/main/src/libcamera/control_ids_core.yaml>
- [Libcamera Official Documentation](https://libcamera.org/)
- [Raspberry Pi Camera Algorithm and Tuning Guide](https://datasheets.raspberrypi.com/camera/raspberry-pi-camera-guide.pdf)