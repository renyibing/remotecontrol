"""A class for managing Momo processes."""

import json
import platform
import shlex
import subprocess
import time
from enum import StrEnum
from pathlib import Path
from types import TracebackType
from typing import Any, Literal, Self

import httpx


class MomoMode(StrEnum):
    """Momo operation modes."""

    P2P = "p2p"
    AYAME = "ayame"
    SORA = "sora"


class Momo:
    """A class for managing Momo processes."""

    def __init__(
        self,
        # Mode setting (required).
        mode: MomoMode,
        # === Common options ===
        no_google_stun: bool = False,
        no_video_device: bool = False,
        no_audio_device: bool = False,
        fake_capture_device: bool = True,
        force_i420: bool = False,
        force_yuy2: bool = False,
        force_nv12: bool = False,
        hw_mjpeg_decoder: bool | None = None,
        use_libcamera: bool = False,
        use_libcamera_native: bool = False,
        libcamera_control: list[tuple[str, str]] | None = None,
        video_device: str | None = None,
        resolution: str | None = None,  # QVGA, VGA, HD, FHD, 4K, or [WIDTH]x[HEIGHT]
        framerate: int | None = None,  # 1-60
        fixed_resolution: bool = False,
        priority: Literal["BALANCE", "FRAMERATE", "RESOLUTION"] | None = None,
        use_sdl: bool = False,
        window_width: int | None = None,  # 180-16384
        window_height: int | None = None,  # 180-16384
        fullscreen: bool = False,
        version: bool = False,
        insecure: bool = False,
        log_level: Literal["verbose", "info", "warning", "error", "none"] | None = None,
        screen_capture: bool = False,
        disable_echo_cancellation: bool = False,
        disable_auto_gain_control: bool = False,
        disable_noise_suppression: bool = False,
        disable_highpass_filter: bool = False,
        video_codec_engines: bool = False,
        # Codec settings.
        vp8_encoder: Literal["default", "software"] | None = None,
        vp8_decoder: Literal["default", "software"] | None = None,
        vp9_encoder: Literal["default", "vpl", "software"] | None = None,
        vp9_decoder: Literal["default", "vpl", "nvidia", "software"] | None = None,
        av1_encoder: Literal["default", "vpl", "nvidia", "software"] | None = None,
        av1_decoder: Literal["default", "vpl", "nvidia", "software"] | None = None,
        h264_encoder: Literal["default", "vpl", "nvidia", "videotoolbox", "software"] | None = None,
        h264_decoder: Literal["default", "vpl", "nvidia", "videotoolbox"] | None = None,
        h265_encoder: Literal["default", "vpl", "nvidia", "videotoolbox"] | None = None,
        h265_decoder: Literal["default", "vpl", "nvidia", "videotoolbox"] | None = None,
        openh264: str | None = None,  # File path (automatically obtained from the OPENH264_PATH environment variable).
        # Other common settings.
        serial: str | None = None,  # [DEVICE],[BAUDRATE]
        metrics_port: int = 9090,
        metrics_allow_external_ip: bool = False,
        client_cert: str | None = None,  # PEM file path
        client_key: str | None = None,  # PEM file path
        proxy_url: str | None = None,
        proxy_username: str | None = None,
        proxy_password: str | None = None,
        # === p2p mode specific ===
        document_root: str | None = None,  # Directory path
        port: int | None = None,  # p2p mode port
        # === ayame mode specific ===
        ayame_signaling_url: str | None = None,
        room_id: str | None = None,
        client_id: str | None = None,
        signaling_key: str | None = None,
        direction: Literal["sendrecv", "sendonly", "recvonly"] | None = None,
        ayame_video_codec_type: Literal["VP8", "VP9", "AV1", "H264", "H265"] | None = None,
        ayame_audio_codec_type: Literal["OPUS", "PCMU", "PCMA"] | None = None,
        # === sora mode specific ===
        signaling_urls: str | None = None,  # Multiple URLs (space-separated).
        channel_id: str | None = None,
        auto: bool | None = None,
        video: bool | None = None,
        audio: bool | None = None,
        video_codec_type: Literal["VP8", "VP9", "AV1", "H264", "H265"] | None = None,
        audio_codec_type: Literal["OPUS"] | None = None,
        video_bit_rate: int | None = None,  # 0-30000
        audio_bit_rate: int | None = None,  # 0-510
        role: Literal["sendonly", "recvonly", "sendrecv"] | None = None,
        spotlight: bool | None = None,
        spotlight_number: int | None = None,  # 0-8
        sora_port: int | None = None,  # -1-65535
        simulcast: bool | None = None,
        data_channel_signaling: Literal["true", "false", "none"] | None = None,
        data_channel_signaling_timeout: int | None = None,
        ignore_disconnect_websocket: Literal["true", "false", "none"] | None = None,
        disconnect_wait_timeout: int | None = None,
        metadata: dict[str, Any] | None = None,
        # Other custom arguments.
        extra_args: list[str] | None = None,
        # Startup wait time.
        initial_wait: int | None = None,
    ) -> None:
        """
        A class for managing Momo processes.

        Examples:
            with Momo(
                mode=MomoMode.P2P,
                resolution="HD"
            ) as m:
                # Test code
                response = http_client.get(f"http://localhost:{m.metrics_port}/metrics")

            # Sora mode example
            with Momo(
                mode=MomoMode.SORA,
                signaling_urls="wss://sora.example.com/signaling wss://sora2.example.com/signaling",  # Multiple URLs (space-separated).
                channel_id="test-channel",
                role="sendonly",
                video_codec_type="H264",
            ) as m:
                # Test code
        """
        # Automatically detect the path to the executable file.
        self.executable_path = self._get_momo_executable_path()
        self.process: subprocess.Popen[Any] | None = None
        self.metrics_port = metrics_port
        # Set the default initial wait time.
        self.initial_wait = initial_wait if initial_wait is not None else 2

        # Save all arguments.
        self.kwargs: dict[str, Any] = {
            "mode": mode,
            "no_google_stun": no_google_stun,
            "no_video_device": no_video_device,
            "no_audio_device": no_audio_device,
            "fake_capture_device": fake_capture_device,
            "force_i420": force_i420,
            "force_yuy2": force_yuy2,
            "force_nv12": force_nv12,
            "hw_mjpeg_decoder": hw_mjpeg_decoder,
            "use_libcamera": use_libcamera,
            "use_libcamera_native": use_libcamera_native,
            "libcamera_control": libcamera_control,
            "video_device": video_device,
            "resolution": resolution,
            "framerate": framerate,
            "fixed_resolution": fixed_resolution,
            "priority": priority,
            "use_sdl": use_sdl,
            "window_width": window_width,
            "window_height": window_height,
            "fullscreen": fullscreen,
            "version": version,
            "insecure": insecure,
            "log_level": log_level,
            "screen_capture": screen_capture,
            "disable_echo_cancellation": disable_echo_cancellation,
            "disable_auto_gain_control": disable_auto_gain_control,
            "disable_noise_suppression": disable_noise_suppression,
            "disable_highpass_filter": disable_highpass_filter,
            "video_codec_engines": video_codec_engines,
            "vp8_encoder": vp8_encoder,
            "vp8_decoder": vp8_decoder,
            "vp9_encoder": vp9_encoder,
            "vp9_decoder": vp9_decoder,
            "av1_encoder": av1_encoder,
            "av1_decoder": av1_decoder,
            "h264_encoder": h264_encoder,
            "h264_decoder": h264_decoder,
            "h265_encoder": h265_encoder,
            "h265_decoder": h265_decoder,
            "openh264": openh264,
            "serial": serial,
            "metrics_port": metrics_port,
            "metrics_allow_external_ip": metrics_allow_external_ip,
            "client_cert": client_cert,
            "client_key": client_key,
            "proxy_url": proxy_url,
            "proxy_username": proxy_username,
            "proxy_password": proxy_password,
            "document_root": document_root,
            "port": port,
            "ayame_signaling_url": ayame_signaling_url,
            "room_id": room_id,
            "client_id": client_id,
            "signaling_key": signaling_key,
            "direction": direction,
            "ayame_video_codec_type": ayame_video_codec_type,
            "ayame_audio_codec_type": ayame_audio_codec_type,
            "signaling_urls": signaling_urls,
            "channel_id": channel_id,
            "auto": auto,
            "video": video,
            "audio": audio,
            "video_codec_type": video_codec_type,
            "audio_codec_type": audio_codec_type,
            "video_bit_rate": video_bit_rate,
            "audio_bit_rate": audio_bit_rate,
            "role": role,
            "spotlight": spotlight,
            "spotlight_number": spotlight_number,
            "sora_port": sora_port,
            "simulcast": simulcast,
            "data_channel_signaling": data_channel_signaling,
            "data_channel_signaling_timeout": data_channel_signaling_timeout,
            "ignore_disconnect_websocket": ignore_disconnect_websocket,
            "disconnect_wait_timeout": disconnect_wait_timeout,
            "metadata": metadata,
            "extra_args": extra_args,
        }

        # Execute mode-specific option validation.
        self._validate_mode_options(mode, self.kwargs)

        # Initialize the HTTP client (None).
        self._http_client: httpx.Client | None = None

    def _get_momo_executable_path(self) -> str:
        """Automatically detect the path to the built momo executable file."""
        project_root = Path(__file__).parent.parent
        build_dir = project_root / "_build"

        # Detect the actual build target in the _build directory.
        if not build_dir.exists():
            raise RuntimeError(
                f"Build directory {build_dir} does not exist. "
                f"Please build with: python3 run.py build <target>"
            )

        available_targets = [
            d.name
            for d in build_dir.iterdir()
            if d.is_dir() and (d / "release" / "momo" / "momo").exists()
        ]

        if not available_targets:
            raise RuntimeError(
                f"No built momo executables found in {build_dir}. "
                f"Please build with: python3 run.py build <target>"
            )

        if len(available_targets) == 1:
            # If there is only one build, automatically select it.
            target = available_targets[0]
            print(f"Auto-detected momo target: {target}")
        else:
            # If there are multiple builds, determine the priority according to the platform.
            system = platform.system().lower()
            machine = platform.machine().lower()

            # Priority list according to the platform.
            if system == "darwin":
                if machine == "arm64" or machine == "aarch64":
                    preferred = ["macos_arm64", "macos_x86_64"]
                else:
                    preferred = ["macos_x86_64", "macos_arm64"]
            elif system == "linux":
                if machine == "aarch64":
                    preferred = ["ubuntu-24.04_armv8", "ubuntu-22.04_armv8", "ubuntu-20.04_armv8"]
                else:
                    preferred = [
                        "ubuntu-24.04_x86_64",
                        "ubuntu-22.04_x86_64",
                        "ubuntu-20.04_x86_64",
                    ]
            else:
                preferred = []

            # Select according to the priority.
            target = None
            for pref in preferred:
                if pref in available_targets:
                    target = pref
                    print(
                        f"Auto-detected momo target: {target} (from {len(available_targets)} available)"
                    )
                    break

            if not target:
                # If no target is found according to the priority, use the first one.
                target = available_targets[0]
                print(
                    f"Using first available target: {target} (available: {', '.join(available_targets)})"
                )

        # Build the path to the momo executable.
        assert target is not None
        momo_path = project_root / "_build" / target / "release" / "momo" / "momo"

        if not momo_path.exists():
            raise RuntimeError(
                f"momo executable not found at {momo_path}. "
                f"Please build with: python3 run.py build {target}"
            )

        return str(momo_path)

    def __enter__(self) -> Self:
        """Start the context manager."""
        try:
            # Build the command line arguments.
            args = self._build_args(**self.kwargs)

            # Display the startup command.
            cmd = [self.executable_path] + args
            # Use shlex.quote to properly display arguments containing JSON.
            quoted_cmd = " ".join(shlex.quote(arg) for arg in cmd)
            print(f"Starting momo with command: {quoted_cmd}")

            # Start the process (capture error output to check for problems).
            self.process = subprocess.Popen(
                cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
            print(f"Started momo process with PID: {self.process.pid}")

            # Wait until the process starts and the metrics are available.
            self._wait_for_startup(self.metrics_port, timeout=30, initial_wait=self.initial_wait)

            # Create the HTTP client.
            self._http_client = httpx.Client(timeout=10.0)

            return self
        except Exception as e:
            # If an exception occurs, always clean up.
            if self.process:
                print(f"Cleaning up momo process (PID: {self.process.pid}) due to exception: {e}")
            self._cleanup()
            raise

    def __exit__(
        self,
        _exc_type: type[BaseException] | None,
        _exc_val: BaseException | None,
        _exc_tb: TracebackType | None,
    ) -> Literal[False]:
        """End the context manager."""
        # Clean up the HTTP client.
        if self._http_client:
            self._http_client.close()
            self._http_client = None

        self._cleanup()
        return False

    def _build_args(self, mode: MomoMode, **kwargs: Any) -> list[str]:
        """Build the command line arguments."""
        # Validate the mode-specific options.
        self._validate_mode_options(mode, kwargs)

        args = []

        # === Common options (placed before the mode) ===

        # Basic flags
        if kwargs.get("no_google_stun"):
            args.append("--no-google-stun")
        if kwargs.get("no_video_device"):
            args.append("--no-video-input-device")
        if kwargs.get("no_audio_device"):
            args.append("--no-audio-device")
        if kwargs.get("fake_capture_device"):
            args.append("--fake-capture-device")
        if kwargs.get("force_i420"):
            args.append("--force-i420")
        if kwargs.get("force_yuy2"):
            args.append("--force-yuy2")
        if kwargs.get("force_nv12"):
            args.append("--force-nv12")
        if kwargs.get("hw_mjpeg_decoder") is not None:
            args.extend(["--hw-mjpeg-decoder", str(int(kwargs["hw_mjpeg_decoder"]))])
        if kwargs.get("use_libcamera"):
            args.append("--use-libcamera")
        if kwargs.get("use_libcamera_native"):
            args.append("--use-libcamera-native")

        # libcamera control
        if kwargs.get("libcamera_control"):
            for key, value in kwargs["libcamera_control"]:
                args.extend(["--libcamera-control", key, value])

        # Video settings
        if kwargs.get("video_device"):
            args.extend(["--video-input-device", kwargs["video_device"]])
        if kwargs.get("resolution"):
            args.extend(["--resolution", kwargs["resolution"]])
        if kwargs.get("framerate") is not None:
            args.extend(["--framerate", str(kwargs["framerate"])])
        if kwargs.get("fixed_resolution"):
            args.append("--fixed-resolution")
        if kwargs.get("priority"):
            args.extend(["--priority", kwargs["priority"]])

        # SDL settings
        if kwargs.get("use_sdl"):
            args.append("--use-sdl")
        if kwargs.get("window_width") is not None:
            args.extend(["--window-width", str(kwargs["window_width"])])
        if kwargs.get("window_height") is not None:
            args.extend(["--window-height", str(kwargs["window_height"])])
        if kwargs.get("fullscreen"):
            args.append("--fullscreen")

        # Other basic settings
        if kwargs.get("version"):
            args.append("--version")
        if kwargs.get("insecure"):
            args.append("--insecure")
        if kwargs.get("log_level"):
            args.extend(["--log-level", kwargs["log_level"]])
        if kwargs.get("screen_capture"):
            args.append("--screen-capture")

        # Audio settings
        if kwargs.get("disable_echo_cancellation"):
            args.append("--disable-echo-cancellation")
        if kwargs.get("disable_auto_gain_control"):
            args.append("--disable-auto-gain-control")
        if kwargs.get("disable_noise_suppression"):
            args.append("--disable-noise-suppression")
        if kwargs.get("disable_highpass_filter"):
            args.append("--disable-highpass-filter")

        # Codec settings
        if kwargs.get("video_codec_engines"):
            args.append("--video-codec-engines")
        if kwargs.get("vp8_encoder"):
            args.extend(["--vp8-encoder", kwargs["vp8_encoder"]])
        if kwargs.get("vp8_decoder"):
            args.extend(["--vp8-decoder", kwargs["vp8_decoder"]])
        if kwargs.get("vp9_encoder"):
            args.extend(["--vp9-encoder", kwargs["vp9_encoder"]])
        if kwargs.get("vp9_decoder"):
            args.extend(["--vp9-decoder", kwargs["vp9_decoder"]])
        if kwargs.get("av1_encoder"):
            args.extend(["--av1-encoder", kwargs["av1_encoder"]])
        if kwargs.get("av1_decoder"):
            args.extend(["--av1-decoder", kwargs["av1_decoder"]])
        if kwargs.get("h264_encoder"):
            args.extend(["--h264-encoder", kwargs["h264_encoder"]])
        if kwargs.get("h264_decoder"):
            args.extend(["--h264-decoder", kwargs["h264_decoder"]])
        if kwargs.get("h265_encoder"):
            args.extend(["--h265-encoder", kwargs["h265_encoder"]])
        if kwargs.get("h265_decoder"):
            args.extend(["--h265-decoder", kwargs["h265_decoder"]])
        if kwargs.get("openh264"):
            args.extend(["--openh264", kwargs["openh264"]])

        # Other common settings
        if kwargs.get("serial"):
            args.extend(["--serial", kwargs["serial"]])
        if kwargs.get("metrics_port") is not None and kwargs["metrics_port"] != -1:
            args.extend(["--metrics-port", str(kwargs["metrics_port"])])
        if kwargs.get("metrics_allow_external_ip"):
            args.append("--metrics-allow-external-ip")
        if kwargs.get("client_cert"):
            args.extend(["--client-cert", kwargs["client_cert"]])
        if kwargs.get("client_key"):
            args.extend(["--client-key", kwargs["client_key"]])
        if kwargs.get("proxy_url"):
            args.extend(["--proxy-url", kwargs["proxy_url"]])
        if kwargs.get("proxy_username"):
            args.extend(["--proxy-username", kwargs["proxy_username"]])
        if kwargs.get("proxy_password"):
            args.extend(["--proxy-password", kwargs["proxy_password"]])

        # === Mode specification and mode-specific options ===

        if mode == MomoMode.P2P:
            args.append(mode.value)
            if kwargs.get("document_root"):
                args.extend(["--document-root", kwargs["document_root"]])
            # The default port for p2p mode is 8080
            port = kwargs.get("port") if kwargs.get("port") is not None else 8080
            args.extend(["--port", str(port)])

        elif mode == MomoMode.AYAME:
            args.append(mode.value)
            if kwargs.get("ayame_signaling_url"):
                args.extend(["--signaling-url", kwargs["ayame_signaling_url"]])
            if kwargs.get("room_id"):
                args.extend(["--room-id", kwargs["room_id"]])
            if kwargs.get("client_id"):
                args.extend(["--client-id", kwargs["client_id"]])
            if kwargs.get("signaling_key"):
                args.extend(["--signaling-key", kwargs["signaling_key"]])
            if kwargs.get("direction"):
                args.extend(["--direction", kwargs["direction"]])
            if kwargs.get("ayame_video_codec_type"):
                args.extend(["--video-codec-type", kwargs["ayame_video_codec_type"]])
            if kwargs.get("ayame_audio_codec_type"):
                args.extend(["--audio-codec-type", kwargs["ayame_audio_codec_type"]])

        elif mode == MomoMode.SORA:
            args.append(mode.value)
            if kwargs.get("signaling_urls"):
                # Multiple URL support (if the string is split by spaces).
                if isinstance(kwargs["signaling_urls"], list):
                    args.extend(["--signaling-urls"] + kwargs["signaling_urls"])
                else:
                    # If the string is split by spaces, pass it as multiple arguments.
                    urls = kwargs["signaling_urls"].split()
                    args.extend(["--signaling-urls"] + urls)
            if kwargs.get("channel_id"):
                args.extend(["--channel-id", kwargs["channel_id"]])
            if kwargs.get("auto"):
                args.append("--auto")
            # The default for sora mode is video/audio both true
            video = kwargs.get("video") if kwargs.get("video") is not None else True
            audio = kwargs.get("audio") if kwargs.get("audio") is not None else True
            args.extend(["--video", str(video).lower()])
            args.extend(["--audio", str(audio).lower()])
            if kwargs.get("video_codec_type"):
                args.extend(["--video-codec-type", kwargs["video_codec_type"]])
            if kwargs.get("audio_codec_type"):
                args.extend(["--audio-codec-type", kwargs["audio_codec_type"]])
            if kwargs.get("video_bit_rate") is not None:
                args.extend(["--video-bit-rate", str(kwargs["video_bit_rate"])])
            if kwargs.get("audio_bit_rate") is not None:
                args.extend(["--audio-bit-rate", str(kwargs["audio_bit_rate"])])
            if kwargs.get("role"):
                args.extend(["--role", kwargs["role"]])
            if kwargs.get("spotlight") is not None:
                args.extend(["--spotlight", str(int(kwargs["spotlight"]))])
            if kwargs.get("spotlight_number") is not None:
                args.extend(["--spotlight-number", str(kwargs["spotlight_number"])])
            if kwargs.get("sora_port") is not None:
                args.extend(["--port", str(kwargs["sora_port"])])
            if kwargs.get("simulcast") is not None:
                args.extend(["--simulcast", str(kwargs["simulcast"]).lower()])
            if kwargs.get("data_channel_signaling"):
                args.extend(["--data-channel-signaling", kwargs["data_channel_signaling"]])
            if kwargs.get("data_channel_signaling_timeout") is not None:
                args.extend(
                    [
                        "--data-channel-signaling-timeout",
                        str(kwargs["data_channel_signaling_timeout"]),
                    ]
                )
            if kwargs.get("ignore_disconnect_websocket"):
                args.extend(
                    ["--ignore-disconnect-websocket", kwargs["ignore_disconnect_websocket"]]
                )
            if kwargs.get("disconnect_wait_timeout") is not None:
                args.extend(["--disconnect-wait-timeout", str(kwargs["disconnect_wait_timeout"])])
            if kwargs.get("metadata"):
                args.extend(["--metadata", json.dumps(kwargs["metadata"])])

        # Other custom arguments
        if kwargs.get("extra_args"):
            args.extend(kwargs["extra_args"])

        return args

    def _validate_mode_options(self, mode: MomoMode, kwargs: dict[str, Any]) -> None:
        """Validate the mode-specific options."""
        # p2p mode specific options
        p2p_only_options = {"document_root"}

        # ayame mode specific options
        ayame_only_options = {
            "ayame_signaling_url",
            "room_id",
            "client_id",
            "signaling_key",
            "direction",
            "ayame_video_codec_type",
            "ayame_audio_codec_type",
        }

        # sora mode specific options
        sora_only_options = {
            "signaling_urls",
            "channel_id",
            "auto",
            "video",
            "audio",
            "video_codec_type",
            "audio_codec_type",
            "video_bit_rate",
            "audio_bit_rate",
            "role",
            "spotlight",
            "spotlight_number",
            "sora_port",
            "simulcast",
            "data_channel_signaling",
            "data_channel_signaling_timeout",
            "ignore_disconnect_websocket",
            "disconnect_wait_timeout",
            "metadata",
        }

        # Get the keys of the specified options (not None).
        specified_options = {k for k, v in kwargs.items() if v is not None}

        # Validate for each mode.
        if mode == MomoMode.P2P:
            # If ayame/sora options are specified in p2p mode, an error occurs.
            invalid_options = specified_options & (ayame_only_options | sora_only_options)
            if invalid_options:
                # Determine which mode the options belong to.
                modes = []
                if invalid_options & ayame_only_options:
                    modes.append("ayame")
                if invalid_options & sora_only_options:
                    modes.append("sora")
                raise ValueError(
                    f"Invalid options specified for P2P mode: {', '.join(sorted(invalid_options))}\n"
                    f"These options are only for {'/'.join(modes)} mode"
                )

        elif mode == MomoMode.AYAME:
            # If p2p/sora options are specified in ayame mode, an error occurs.
            invalid_options = specified_options & (p2p_only_options | sora_only_options)
            if invalid_options:
                # Determine which mode the options belong to.
                modes = []
                if invalid_options & p2p_only_options:
                    modes.append("p2p")
                if invalid_options & sora_only_options:
                    modes.append("sora")
                raise ValueError(
                    f"Invalid options specified for Ayame mode: {', '.join(sorted(invalid_options))}\n"
                    f"These options are only for {'/'.join(modes)} mode"
                )

        elif mode == MomoMode.SORA:
            # If p2p/ayame options are specified in sora mode, an error occurs.
            invalid_options = specified_options & (p2p_only_options | ayame_only_options)
            if invalid_options:
                # Determine which mode the options belong to.
                modes = []
                if invalid_options & p2p_only_options:
                    modes.append("p2p")
                if invalid_options & ayame_only_options:
                    modes.append("ayame")
                raise ValueError(
                    f"Invalid options specified for Sora mode: {', '.join(sorted(invalid_options))}\n"
                    f"These options are only for {'/'.join(modes)} mode"
                )

    def _wait_for_startup(
        self, metrics_port: int, timeout: int = 30, initial_wait: int = 2
    ) -> None:
        """Wait for the process to start and the metrics to be available."""
        if not self.process:
            raise RuntimeError("Process not started")

        # Wait for the process to completely start.
        # It may fail to check the metrics endpoint immediately.
        if initial_wait > 0:
            time.sleep(initial_wait)

        print(
            f"Waiting for metrics endpoint to be ready on port {metrics_port} (timeout: {timeout}s)..."
        )
        start_time = time.time()

        with httpx.Client() as client:
            while time.time() - start_time < timeout:
                # Check the process status.
                if self.process.poll() is not None:
                    # If the process has exited, an error occurs.
                    error_msg = (
                        f"momo process exited unexpectedly with code {self.process.returncode}"
                    )
                    # Display the stderr content.
                    if hasattr(self.process, "stderr") and self.process.stderr:
                        stderr_output = self.process.stderr.read()
                        if stderr_output:
                            error_msg += f"\nStderr output:\n{stderr_output}"
                    raise RuntimeError(error_msg)

                # Check the metrics endpoint.
                try:
                    url = f"http://localhost:{metrics_port}/metrics"
                    response = client.get(url, timeout=5)
                    if response.status_code == 200:
                        # For Sora mode, check that stats is not empty.
                        if self.kwargs["mode"] == MomoMode.SORA:
                            data = response.json()
                            stats = data.get("stats", [])
                            if stats and len(stats) > 0:
                                print(
                                    f"Momo started successfully after {time.time() - start_time:.1f}s (stats count: {len(stats)})"
                                )
                                return
                            else:
                                # If stats is empty, continue waiting.
                                elapsed = time.time() - start_time
                                print(
                                    f"  Metrics endpoint is up but stats is empty, waiting for connection... ({elapsed:.1f}s elapsed)"
                                )
                        else:
                            # p2p/ayame mode is successful with a 200 response (no need to check stats).
                            print(
                                f"Momo started successfully after {time.time() - start_time:.1f}s"
                            )
                            return
                    else:
                        print(f"  Got status code: {response.status_code}")
                except httpx.ConnectError:
                    # Ignore connection errors and continue to the next attempt.
                    elapsed = time.time() - start_time
                    if elapsed > 5 and int(elapsed) % 5 == 0:  # Output status every 5 seconds.
                        print(
                            f"  Still waiting for metrics on port {metrics_port} ({elapsed:.1f}s elapsed)"
                        )
                        # Check stderr non-blocking.
                        if hasattr(self.process, "stderr") and self.process.stderr:
                            import select

                            # Check if there is readable data in stderr.
                            if select.select([self.process.stderr], [], [], 0)[0]:
                                # Set non-blocking mode.
                                import fcntl
                                import os

                                fd = self.process.stderr.fileno()
                                fl = fcntl.fcntl(fd, fcntl.F_GETFL)
                                fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)
                                try:
                                    stderr_chunk = self.process.stderr.read(4096)
                                    if stderr_chunk:
                                        print(f"  Stderr (during startup): {stderr_chunk}")
                                except IOError:
                                    pass
                                # Reset.
                                fcntl.fcntl(fd, fcntl.F_SETFL, fl)
                    pass
                except httpx.ConnectTimeout:
                    pass
                except httpx.HTTPStatusError as e:
                    print(f"  HTTP error: {e}")

                # Wait for the next attempt for 1 second.
                time.sleep(1)

            # Timeout.
            if self.process:
                print(f"Timeout waiting for momo process (PID: {self.process.pid}) to start")
                # Display the stderr content.
                if hasattr(self.process, "stderr") and self.process.stderr:
                    print("Checking for stderr output...")
                    stderr_output = self.process.stderr.read()
                    if stderr_output:
                        print(f"Stderr output:\n{stderr_output}")
            self._cleanup()
            raise RuntimeError(f"momo process failed to start within {timeout} seconds")

    def _cleanup(self) -> None:
        """Clean up the process."""
        if self.process:
            pid = self.process.pid
            print(f"Terminating momo process (PID: {pid})")
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
                print(f"Momo process (PID: {pid}) terminated gracefully")
            except subprocess.TimeoutExpired:
                print(f"Force killing momo process (PID: {pid})")
                self.process.kill()
                self.process.wait()
                print(f"Momo process (PID: {pid}) killed")
            self.process = None

            # Short wait after process termination (for resource release).
            import time

            time.sleep(0.2)

    def get_metrics(
        self,
        wait_stats: list[dict[str, Any]] | None = None,
        wait_stats_timeout: int = 5,
        wait_after_stats: float = 0,
        interval: float = 0.5,
    ) -> dict[str, Any]:
        """
        Get the metrics.

        Args:
            wait_stats: Wait until this statistics is entered.
                        Each element is a dictionary of the expected statistics. Wait for statistics that match all items including type.
            wait_stats_timeout: Timeout time for waiting for statistics (seconds). Default 5 seconds.
            wait_after_stats: Additional wait time after all statistics conditions are met (seconds). Default 0 seconds.
                             Use this if you want to wait for statistics to accumulate.
            interval: Check interval (seconds)

        Returns:
            Dictionary of metrics information

        Raises:
            RuntimeError: Timeout, or HTTP client not initialized

        Examples:
            # Normal usage (immediately get metrics)
            metrics = momo.get_metrics()

            # Wait until specific statistics is entered.
            metrics = momo.get_metrics(wait_stats=[
                {"type": "candidate-pair", "state": "succeeded"},
                {"type": "outbound-rtp", "rid": "r0", "encoderImplementation": "libvpx"},
            ])

            # Set timeout to 10 seconds.
            metrics = momo.get_metrics(
                wait_stats=[{"type": "codec", "mimeType": "video/H264"}],
                wait_stats_timeout=10
            )

            # Wait for 2 seconds after all statistics are met.
            metrics = momo.get_metrics(
                wait_stats=[{"type": "codec", "mimeType": "video/H264"}],
                wait_after_stats=2
            )
        """
        if not self._http_client:
            raise RuntimeError("HTTP client not initialized")
        if not self.metrics_port:
            raise RuntimeError("Process not started")

        # If statistics waiting is not needed, return the metrics immediately.
        if not wait_stats:
            response = self._http_client.get(f"http://localhost:{self.metrics_port}/metrics")
            response.raise_for_status()
            return response.json()

        # Wait for statistics.
        start_time = time.time()
        while time.time() - start_time < wait_stats_timeout:
            try:
                response = self._http_client.get(f"http://localhost:{self.metrics_port}/metrics")
                if response.status_code == 200:
                    data = response.json()
                    stats = data.get("stats", [])

                    # Check the waiting conditions - check if there is a statistics that matches all expected dictionaries.
                    all_conditions_met = True
                    for expected_dict in wait_stats:
                        # Search for a statistics that matches the expected dictionary exactly.
                        found = False
                        for stat in stats:
                            # First, check if type matches.
                            if stat.get("type") != expected_dict.get("type"):
                                continue

                            # Check if all items in expected_dict are included in stat and the values match.
                            all_items_match = True
                            for key, expected_value in expected_dict.items():
                                if stat.get(key) != expected_value:
                                    all_items_match = False
                                    break

                            if all_items_match:
                                found = True
                                break

                        if not found:
                            all_conditions_met = False
                            break

                    if all_conditions_met:
                        # If all statistics conditions are met, wait for the specified time.
                        if wait_after_stats > 0:
                            time.sleep(wait_after_stats)
                        return data

            except (httpx.ConnectError, httpx.HTTPStatusError):
                pass

            time.sleep(interval)

        # Timeout.
        raise RuntimeError(
            f"Timeout waiting for expected stats within {wait_stats_timeout} seconds. "
            f"Expected stats: {wait_stats}"
        )

    def wait_for_connection(
        self,
        timeout: int = 10,
        interval: float = 0.5,
    ) -> bool:
        """
        Wait for the connection to be established.

        Args:
            timeout: Timeout time for the connection to be established (seconds)
            interval: Check interval (seconds)

        Returns:
            True if the connection is established, False if timeout

        Examples:
            # Default behavior (check the connection of transport)
            momo.wait_for_connection()

            # Set timeout to 20 seconds.
            momo.wait_for_connection(timeout=20)
        """
        if not self._http_client:
            raise RuntimeError("HTTP client not initialized")

        # Wait for the connection to be established (default conditions).
        default_conditions = [
            {"type": "transport", "key": "dtlsState", "value": "connected"},
            {"type": "transport", "key": "iceState", "value": "connected"},
        ]

        start_time = time.time()

        # Wait for the connection to be established.
        while time.time() - start_time < timeout:
            try:
                response = self._http_client.get(f"http://localhost:{self.metrics_port}/metrics")
                if response.status_code == 200:
                    data = response.json()
                    stats = data.get("stats", [])

                    # Check the default conditions.
                    all_conditions_met = True
                    for expected in default_conditions:
                        expected_type = expected.get("type")
                        expected_key = expected.get("key")
                        expected_value = expected.get("value")

                        # Search for a statistics that matches the key and value in the same type.
                        found = False
                        for stat in stats:
                            if stat.get("type") == expected_type:
                                if stat.get(expected_key) == expected_value:
                                    found = True
                                    break

                        if not found:
                            all_conditions_met = False
                            break

                    if all_conditions_met:
                        return True

            except (httpx.ConnectError, httpx.HTTPStatusError):
                pass

            time.sleep(interval)

        return False  # Timeout.
