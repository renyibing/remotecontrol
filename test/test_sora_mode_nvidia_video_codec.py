import os

import pytest

from momo import Momo, MomoMode

# Skip Sora mode tests if TEST_SORA_MODE_SIGNALING_URLS is not set.
# Skip if NVIDIA Video Codec environment is not enabled.
pytestmark = pytest.mark.skipif(
    not os.environ.get("TEST_SORA_MODE_SIGNALING_URLS") or not os.environ.get("NVIDIA_VIDEO_CODEC"),
    reason="TEST_SORA_MODE_SIGNALING_URLS or NVIDIA_VIDEO_CODEC not set in environment",
)


@pytest.mark.parametrize(
    "video_codec_type",
    [
        "AV1",
        "H264",
        "H265",
    ],
)
def test_connection_stats(sora_settings, video_codec_type, free_port):
    """Confirm that connection statistics are collected in Sora mode."""
    # Generate expected_mime_type.
    expected_mime_type = f"video/{video_codec_type}"

    # Prepare encoder settings.
    encoder_params = {}
    if video_codec_type == "AV1":
        encoder_params["av1_encoder"] = "nvidia"
    elif video_codec_type == "H264":
        encoder_params["h264_encoder"] = "nvidia"
    elif video_codec_type == "H265":
        encoder_params["h265_encoder"] = "nvidia"

    with Momo(
        fake_capture_device=True,
        metrics_port=free_port,
        mode=MomoMode.SORA,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        audio=True,
        video=True,
        video_codec_type=video_codec_type,
        metadata=sora_settings.metadata,
        initial_wait=10,
        **encoder_params,
    ) as m:
        # Wait for the connection to be established.
        assert m.wait_for_connection(), (
            f"Failed to establish connection for {video_codec_type} codec"
        )

        data = m.get_metrics()
        stats = data["stats"]

        # Sora mode may include connection-related statistics.
        assert stats is not None
        assert isinstance(stats, list)
        assert len(stats) > 0

        # Collect statistics types.
        stat_types = {stat.get("type") for stat in stats if "type" in stat}

        # Confirm that important statistics types exist.
        expected_types = {
            "peer-connection",
            "transport",
            "codec",
            "outbound-rtp",
        }
        for expected_type in expected_types:
            assert expected_type in stat_types

        # Confirm that the specified video codec is actually used.
        codec_mime_types = {
            stat.get("mimeType")
            for stat in stats
            if stat.get("type") == "codec" and "mimeType" in stat
        }
        assert expected_mime_type in codec_mime_types

        # Check the details of each statistics type.
        for stat in stats:
            match stat.get("type"):
                case "outbound-rtp":
                    # Confirm that the required fields of outbound-rtp are present.
                    assert "ssrc" in stat
                    assert "kind" in stat
                    assert "packetsSent" in stat
                    assert "bytesSent" in stat

                    # Confirm that the data is actually being sent.
                    assert stat["packetsSent"] > 0
                    assert stat["bytesSent"] > 0

                    # Confirm that the data is audio/video.
                    match stat["kind"]:
                        case "video":
                            assert "framesEncoded" in stat
                            assert "frameWidth" in stat
                            assert "frameHeight" in stat
                            assert stat["framesEncoded"] > 0

                            # Confirm that the encoder implementation is NVIDIA Video Codec.
                            assert "encoderImplementation" in stat
                            assert stat["encoderImplementation"] == "NvCodec"
                        case "audio":
                            assert "headerBytesSent" in stat
                            assert stat["headerBytesSent"] > 0

                case "codec":
                    # Confirm that the required fields of codec are present.
                    assert "payloadType" in stat
                    assert "mimeType" in stat
                    assert "clockRate" in stat

                    # Confirm that codec is either audio/opus or the specified video codec.
                    assert stat["mimeType"] in ["audio/opus", expected_mime_type]

                    # Check the details of each codec type.
                    if stat["mimeType"] == "audio/opus":
                        assert "channels" in stat
                        assert stat["clockRate"] == 48000
                    elif stat["mimeType"] == expected_mime_type:
                        assert stat["clockRate"] == 90000

                case "transport":
                    # Confirm that the required fields of transport are present.
                    assert "bytesSent" in stat
                    assert "bytesReceived" in stat
                    assert "dtlsState" in stat
                    assert "iceState" in stat

                    # Confirm that the data is actually being sent and received.
                    assert stat["bytesSent"] > 0
                    assert stat["bytesReceived"] > 0

                    # Confirm the connection state.
                    assert stat["dtlsState"] == "connected"
                    assert stat["iceState"] == "connected"

                case "peer-connection":
                    # Confirm that the required fields of peer-connection are present.
                    assert "dataChannelsOpened" in stat


@pytest.mark.parametrize(
    "video_codec_type,expected_encoder_implementation",
    [
        ("AV1", "SimulcastEncoderAdapter (NvCodec, NvCodec, NvCodec)"),
        ("H264", "SimulcastEncoderAdapter (NvCodec, NvCodec, NvCodec)"),
        ("H265", "SimulcastEncoderAdapter (NvCodec, NvCodec, NvCodec)"),
    ],
)
def test_simulcast(sora_settings, video_codec_type, expected_encoder_implementation, free_port):
    """Confirm that simulcast connection statistics are collected in Sora mode (using NVIDIA Video Codec)."""
    # Prepare encoder settings.
    encoder_params = {}
    if video_codec_type == "AV1":
        encoder_params["av1_encoder"] = "nvidia"
    elif video_codec_type == "H264":
        encoder_params["h264_encoder"] = "nvidia"
    elif video_codec_type == "H265":
        encoder_params["h265_encoder"] = "nvidia"

    with Momo(
        mode=MomoMode.SORA,
        metrics_port=free_port,
        fake_capture_device=True,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        audio=True,
        video=True,
        video_codec_type=video_codec_type,
        simulcast=True,
        resolution="960x540",  # 540p resolution
        video_bit_rate=3000,  # 3000 bit rate
        metadata=sora_settings.metadata,
        initial_wait=10,
        **encoder_params,
    ) as m:
        # Wait for the connection to be established.
        assert m.wait_for_connection(), (
            f"Failed to establish connection for {video_codec_type} codec"
        )

        data = m.get_metrics(
            wait_stats=[
                {
                    "type": "outbound-rtp",
                    "rid": "r0",
                    "encoderImplementation": expected_encoder_implementation,
                },
                {
                    "type": "outbound-rtp",
                    "rid": "r1",
                    "encoderImplementation": expected_encoder_implementation,
                },
                {
                    "type": "outbound-rtp",
                    "rid": "r2",
                    "encoderImplementation": expected_encoder_implementation,
                },
            ],
            wait_after_stats=3,
        )
        stats = data["stats"]

        # Sora mode may include connection-related statistics.
        assert stats is not None
        assert isinstance(stats, list)
        assert len(stats) > 0

        # Collect statistics types.
        stat_types = {stat.get("type") for stat in stats if "type" in stat}
        print(f"Available stat types: {stat_types}")

        # Confirm that important statistics types exist.
        expected_types = {
            "peer-connection",
            "transport",
            "codec",
            "outbound-rtp",
        }
        for expected_type in expected_types:
            assert expected_type in stat_types

        # Get and confirm the audio codec.
        audio_codec_stats = [
            stat
            for stat in stats
            if stat.get("type") == "codec" and stat.get("mimeType") == "audio/opus"
        ]
        assert len(audio_codec_stats) == 1, (
            f"Expected 1 audio codec (opus), but got {len(audio_codec_stats)}"
        )

        # Check the details of the audio codec.
        audio_codec = audio_codec_stats[0]
        assert "payloadType" in audio_codec
        assert "mimeType" in audio_codec
        assert "clockRate" in audio_codec
        assert "channels" in audio_codec
        assert audio_codec["clockRate"] == 48000

        # Get and confirm the video codec.
        expected_mime_type = f"video/{video_codec_type}"
        video_codec_stats = [
            stat
            for stat in stats
            if stat.get("type") == "codec" and stat.get("mimeType") == expected_mime_type
        ]
        assert len(video_codec_stats) == 1, (
            f"Expected 1 video codec ({expected_mime_type}), but got {len(video_codec_stats)}"
        )

        # Check the details of the video codec.
        video_codec = video_codec_stats[0]
        assert "payloadType" in video_codec
        assert "mimeType" in video_codec
        assert "clockRate" in video_codec
        assert video_codec["clockRate"] == 90000

        # Get and confirm the audio outbound-rtp.
        audio_outbound_rtp_stats = [
            stat
            for stat in stats
            if stat.get("type") == "outbound-rtp" and stat.get("kind") == "audio"
        ]
        assert len(audio_outbound_rtp_stats) == 1, (
            f"Expected 1 audio outbound-rtp, but got {len(audio_outbound_rtp_stats)}"
        )

        # Check the details of the audio outbound-rtp.
        audio_outbound_rtp = audio_outbound_rtp_stats[0]
        assert "ssrc" in audio_outbound_rtp
        assert "packetsSent" in audio_outbound_rtp
        assert "bytesSent" in audio_outbound_rtp
        assert "headerBytesSent" in audio_outbound_rtp
        assert audio_outbound_rtp["packetsSent"] > 0
        assert audio_outbound_rtp["bytesSent"] > 0
        assert audio_outbound_rtp["headerBytesSent"] > 0

        # Confirm that video outbound-rtp exists for simulcast and there are exactly 3 of them.
        video_outbound_rtp_stats = [
            stat
            for stat in stats
            if stat.get("type") == "outbound-rtp" and stat.get("kind") == "video"
        ]
        assert len(video_outbound_rtp_stats) == 3, (
            f"Expected 3 video outbound-rtp for simulcast, but got {len(video_outbound_rtp_stats)}"
        )

        # Classify by rid.
        video_outbound_rtp_by_rid = {}
        for video_outbound_rtp in video_outbound_rtp_stats:
            rid = video_outbound_rtp.get("rid")
            assert rid in ["r0", "r1", "r2"], f"Unexpected rid: {rid}"
            video_outbound_rtp_by_rid[rid] = video_outbound_rtp

        # Confirm that all rid exist.
        assert set(video_outbound_rtp_by_rid.keys()) == {
            "r0",
            "r1",
            "r2",
        }, f"Expected rid r0, r1, r2, but got {set(video_outbound_rtp_by_rid.keys())}"

        # Check the details of r0 (low resolution).
        outbound_rtp_r0 = video_outbound_rtp_by_rid["r0"]
        assert "ssrc" in outbound_rtp_r0
        assert "rid" in outbound_rtp_r0
        assert outbound_rtp_r0["rid"] == "r0"
        assert "packetsSent" in outbound_rtp_r0
        assert "bytesSent" in outbound_rtp_r0
        assert "framesEncoded" in outbound_rtp_r0
        assert "frameWidth" in outbound_rtp_r0
        assert "frameHeight" in outbound_rtp_r0
        assert outbound_rtp_r0["packetsSent"] > 0
        assert outbound_rtp_r0["bytesSent"] > 0
        assert outbound_rtp_r0["framesEncoded"] > 0

        # Confirm that the encoder implementation of r0 is NVIDIA Video Codec.
        assert "encoderImplementation" in outbound_rtp_r0
        # For NVIDIA Video Codec, SimulcastEncoderAdapter and NvCodec are used.
        assert "SimulcastEncoderAdapter" in outbound_rtp_r0["encoderImplementation"]
        assert "NvCodec" in outbound_rtp_r0["encoderImplementation"]

        assert outbound_rtp_r0["frameWidth"] == 240
        assert outbound_rtp_r0["frameHeight"] == 128
        print(f"r0: {outbound_rtp_r0['frameWidth']}x{outbound_rtp_r0['frameHeight']}")

        # Check the details of r1 (medium resolution).
        outbound_rtp_r1 = video_outbound_rtp_by_rid["r1"]
        assert "ssrc" in outbound_rtp_r1
        assert "rid" in outbound_rtp_r1
        assert outbound_rtp_r1["rid"] == "r1"
        assert "packetsSent" in outbound_rtp_r1
        assert "bytesSent" in outbound_rtp_r1
        assert "framesEncoded" in outbound_rtp_r1
        assert "frameWidth" in outbound_rtp_r1
        assert "frameHeight" in outbound_rtp_r1
        assert outbound_rtp_r1["packetsSent"] > 0
        assert outbound_rtp_r1["bytesSent"] > 0
        assert outbound_rtp_r1["framesEncoded"] > 0

        # Confirm that the encoder implementation of r1 is NVIDIA Video Codec.
        assert "encoderImplementation" in outbound_rtp_r1
        assert "SimulcastEncoderAdapter" in outbound_rtp_r1["encoderImplementation"]
        assert "NvCodec" in outbound_rtp_r1["encoderImplementation"]

        assert outbound_rtp_r1["frameWidth"] == 480
        assert outbound_rtp_r1["frameHeight"] == 256
        print(f"r1: {outbound_rtp_r1['frameWidth']}x{outbound_rtp_r1['frameHeight']}")

        # Check the details of r2 (high resolution).
        outbound_rtp_r2 = video_outbound_rtp_by_rid["r2"]
        assert "ssrc" in outbound_rtp_r2
        assert "rid" in outbound_rtp_r2
        assert outbound_rtp_r2["rid"] == "r2"
        assert "packetsSent" in outbound_rtp_r2
        assert "bytesSent" in outbound_rtp_r2
        assert "framesEncoded" in outbound_rtp_r2
        assert "frameWidth" in outbound_rtp_r2
        assert "frameHeight" in outbound_rtp_r2
        assert outbound_rtp_r2["packetsSent"] > 0
        assert outbound_rtp_r2["bytesSent"] > 0
        assert outbound_rtp_r2["framesEncoded"] > 0

        # Confirm that the encoder implementation of r2 is NVIDIA Video Codec.
        assert "encoderImplementation" in outbound_rtp_r2
        assert "SimulcastEncoderAdapter" in outbound_rtp_r2["encoderImplementation"]
        assert "NvCodec" in outbound_rtp_r2["encoderImplementation"]

        assert outbound_rtp_r2["frameWidth"] == 960
        assert outbound_rtp_r2["frameHeight"] == 528
        print(f"r2: {outbound_rtp_r2['frameWidth']}x{outbound_rtp_r2['frameHeight']}")

        # Check the details of each statistics type (outbound-rtp and codec are already verified above, so skip).
        for stat in stats:
            match stat.get("type"):
                case "transport":
                    # Confirm that the required fields of transport are present.
                    assert "bytesSent" in stat
                    assert "bytesReceived" in stat
                    assert "dtlsState" in stat
                    assert "iceState" in stat

                    # Confirm that the data is actually being sent and received.
                    assert stat["bytesSent"] > 0
                    assert stat["bytesReceived"] > 0

                    # Confirm the connection state.
                    assert stat["dtlsState"] == "connected"
                    assert stat["iceState"] == "connected"

                case "peer-connection":
                    # Confirm that the required fields of peer-connection are present.
                    assert "dataChannelsOpened" in stat


@pytest.mark.parametrize(
    "video_codec_type",
    [
        "AV1",
        "H264",
        "H265",
    ],
)
def test_sora_sendonly_recvonly_pair(
    sora_settings,
    port_allocator,
    video_codec_type,
):
    """Confirm that sendonly and recvonly pair can be created and data can be sent and received in Sora mode (using NVIDIA Video Codec)."""

    # Generate expected_mime_type.
    expected_mime_type = f"video/{video_codec_type}"

    # Prepare encoder settings.
    encoder_params = {}
    if video_codec_type == "AV1":
        encoder_params["av1_encoder"] = "nvidia"
    elif video_codec_type == "H264":
        encoder_params["h264_encoder"] = "nvidia"
    elif video_codec_type == "H265":
        encoder_params["h265_encoder"] = "nvidia"

    # Prepare decoder settings.
    decoder_params = {}
    if video_codec_type == "AV1":
        decoder_params["av1_decoder"] = "nvidia"
    elif video_codec_type == "H264":
        decoder_params["h264_decoder"] = "nvidia"
    elif video_codec_type == "H265":
        decoder_params["h265_decoder"] = "nvidia"

    # Prepare sendonly client.
    with Momo(
        mode=MomoMode.SORA,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        metrics_port=next(port_allocator),
        fake_capture_device=True,
        video=True,
        video_codec_type=video_codec_type,
        audio=True,
        metadata=sora_settings.metadata,
        initial_wait=10,
        **encoder_params,
    ) as sender:
        # Prepare recvonly client.
        with Momo(
            mode=MomoMode.SORA,
            signaling_urls=sora_settings.signaling_urls,
            channel_id=sora_settings.channel_id,
            role="recvonly",
            metrics_port=next(port_allocator),
            video=True,
            audio=True,
            metadata=sora_settings.metadata,
            **decoder_params,
        ) as receiver:
            # Wait for the connection to be established.
            assert sender.wait_for_connection(), (
                f"Sender failed to establish connection for {video_codec_type}"
            )
            assert receiver.wait_for_connection(), (
                f"Receiver failed to establish connection for {video_codec_type}"
            )

            # Check the statistics of the sender.
            sender_data = sender.get_metrics(
                wait_stats=[
                    {
                        "type": "outbound-rtp",
                        "kind": "video",
                        "encoderImplementation": "NvCodec",
                    }
                ]
            )
            sender_stats = sender_data.get("stats", [])

            # Check the statistics of the receiver.
            receiver_data = receiver.get_metrics(
                wait_stats=[
                    {
                        "type": "inbound-rtp",
                        "kind": "video",
                        "decoderImplementation": "NvCodec",
                    }
                ]
            )
            receiver_stats = receiver_data.get("stats", [])

            # Confirm that outbound-rtp exists for audio and video on the sender.
            sender_outbound_rtp = [
                stat for stat in sender_stats if stat.get("type") == "outbound-rtp"
            ]
            assert len(sender_outbound_rtp) == 2, (
                "Sender should have exactly 2 outbound-rtp stats (audio and video)"
            )

            # Confirm that the codec information of the sender (at least 2 for audio and video).
            sender_codecs = [stat for stat in sender_stats if stat.get("type") == "codec"]
            assert len(sender_codecs) >= 2, "Should have at least 2 codecs (audio and video)"

            # Confirm that the mimeType of the video codec.
            sender_video_codec = next(
                (stat for stat in sender_codecs if stat.get("mimeType", "").startswith("video/")),
                None,
            )
            assert sender_video_codec is not None, "Video codec should be present"
            assert sender_video_codec["mimeType"] == expected_mime_type, (
                f"Expected {expected_mime_type}, got {sender_video_codec['mimeType']}"
            )

            # Confirm that the mimeType of the audio codec.
            sender_audio_codec = next(
                (stat for stat in sender_codecs if stat.get("mimeType", "").startswith("audio/")),
                None,
            )
            assert sender_audio_codec is not None, "Audio codec should be present"
            assert sender_audio_codec["mimeType"] == "audio/opus", "Audio codec should be opus"

            # Confirm that the data is being sent on the sender.
            for stat in sender_outbound_rtp:
                assert "packetsSent" in stat
                assert "bytesSent" in stat
                assert stat["packetsSent"] > 0
                assert stat["bytesSent"] > 0

                # Confirm that the encoderImplementation of the video stream is NvCodec.
                if stat.get("kind") == "video":
                    assert "encoderImplementation" in stat
                    assert stat["encoderImplementation"] == "NvCodec"

            # Confirm that inbound-rtp exists for audio and video on the receiver.
            receiver_inbound_rtp = [
                stat for stat in receiver_stats if stat.get("type") == "inbound-rtp"
            ]
            assert len(receiver_inbound_rtp) == 2, (
                "Receiver should have exactly 2 inbound-rtp stats (audio and video)"
            )

            # Confirm that the codec information of the receiver (at least 2 for audio and video).
            receiver_codecs = [stat for stat in receiver_stats if stat.get("type") == "codec"]
            assert len(receiver_codecs) >= 2, (
                "Should have at least 2 codecs (audio and video) on receiver"
            )

            # Confirm that the mimeType of the video codec.
            receiver_video_codec = next(
                (stat for stat in receiver_codecs if stat.get("mimeType", "").startswith("video/")),
                None,
            )
            assert receiver_video_codec is not None, "Video codec should be present on receiver"
            assert receiver_video_codec["mimeType"] == expected_mime_type, (
                f"Expected {expected_mime_type}, got {receiver_video_codec['mimeType']} on receiver"
            )

            # Confirm that the mimeType of the audio codec.
            receiver_audio_codec = next(
                (stat for stat in receiver_codecs if stat.get("mimeType", "").startswith("audio/")),
                None,
            )
            assert receiver_audio_codec is not None, "Audio codec should be present on receiver"
            assert receiver_audio_codec["mimeType"] == "audio/opus", (
                "Audio codec should be opus on receiver"
            )

            # Confirm that the data is being received on the receiver.
            for stat in receiver_inbound_rtp:
                assert "packetsReceived" in stat
                assert "bytesReceived" in stat
                assert stat["packetsReceived"] > 0
                assert stat["bytesReceived"] > 0

                # Confirm that the decoderImplementation of the video stream is NvCodec.
                if stat.get("kind") == "video":
                    assert "decoderImplementation" in stat
                    assert stat["decoderImplementation"] == "NvCodec"
