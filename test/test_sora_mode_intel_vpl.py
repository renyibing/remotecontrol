import os

import pytest

from momo import Momo, MomoMode

# Skip Sora mode tests if TEST_SORA_MODE_SIGNALING_URLS is not set.
# Skip if Intel VPL environment is not enabled.
pytestmark = pytest.mark.skipif(
    not os.environ.get("TEST_SORA_MODE_SIGNALING_URLS") or not os.environ.get("INTEL_VPL"),
    reason="TEST_SORA_MODE_SIGNALING_URLS or INTEL_VPL not set in environment",
)


@pytest.mark.parametrize(
    "video_codec_type",
    [
        "VP9",
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
    match video_codec_type:
        case "VP9":
            encoder_params["vp9_encoder"] = "vpl"
        case "AV1":
            encoder_params["av1_encoder"] = "vpl"
        case "H264":
            encoder_params["h264_encoder"] = "vpl"
        case "H265":
            encoder_params["h265_encoder"] = "vpl"

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

        # Get and confirm the video outbound-rtp.
        video_outbound_rtp_stats = [
            stat
            for stat in stats
            if stat.get("type") == "outbound-rtp" and stat.get("kind") == "video"
        ]
        assert len(video_outbound_rtp_stats) == 1, (
            f"Expected 1 video outbound-rtp, but got {len(video_outbound_rtp_stats)}"
        )

        # Check the details of the video outbound-rtp.
        video_outbound_rtp = video_outbound_rtp_stats[0]
        assert "ssrc" in video_outbound_rtp
        assert "packetsSent" in video_outbound_rtp
        assert "bytesSent" in video_outbound_rtp
        assert "framesEncoded" in video_outbound_rtp
        assert "frameWidth" in video_outbound_rtp
        assert "frameHeight" in video_outbound_rtp
        assert "encoderImplementation" in video_outbound_rtp
        assert video_outbound_rtp["packetsSent"] > 0
        assert video_outbound_rtp["bytesSent"] > 0
        assert video_outbound_rtp["framesEncoded"] > 0
        assert video_outbound_rtp["encoderImplementation"] == "libvpl"

        # Get and confirm the transport.
        transport_stats = [stat for stat in stats if stat.get("type") == "transport"]
        assert len(transport_stats) == 1, f"Expected 1 transport, but got {len(transport_stats)}"

        # Check the details of the transport.
        transport = transport_stats[0]
        assert "bytesSent" in transport
        assert "bytesReceived" in transport
        assert "dtlsState" in transport
        assert "iceState" in transport
        assert transport["bytesSent"] > 0
        assert transport["bytesReceived"] > 0
        assert transport["dtlsState"] == "connected"
        assert transport["iceState"] == "connected"

        # Get and confirm the peer-connection.
        peer_connection_stats = [stat for stat in stats if stat.get("type") == "peer-connection"]
        assert len(peer_connection_stats) == 1, (
            f"Expected 1 peer-connection, but got {len(peer_connection_stats)}"
        )

        # Check the details of the peer-connection.
        peer_connection = peer_connection_stats[0]
        assert "dataChannelsOpened" in peer_connection


@pytest.mark.parametrize(
    "video_codec_type,expected_encoder_implementation",
    [
        ("VP9", "SimulcastEncoderAdapter (libvpl, libvpl, libvpl)"),
        ("AV1", "SimulcastEncoderAdapter (libvpl, libvpl, libvpl)"),
        ("H264", "SimulcastEncoderAdapter (libvpl, libvpl, libvpl)"),
        ("H265", "SimulcastEncoderAdapter (libvpl, libvpl, libvpl)"),
    ],
)
def test_simulcast(sora_settings, video_codec_type, expected_encoder_implementation, free_port):
    """Confirm that statistics are collected in simulcast connection in Sora mode (using Intel VPL)."""
    # Prepare encoder settings.
    encoder_params = {}
    match video_codec_type:
        case "VP9":
            encoder_params["vp9_encoder"] = "vpl"
        case "AV1":
            encoder_params["av1_encoder"] = "vpl"
        case "H264":
            encoder_params["h264_encoder"] = "vpl"
        case "H265":
            encoder_params["h265_encoder"] = "vpl"

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

        # Confirm that there are always 3 video outbound-rtp for simulcast.
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

        # Check the encoder implementation of r0.
        assert "encoderImplementation" in outbound_rtp_r0
        # Intel VPL uses SimulcastEncoderAdapter and libvpl combination.
        assert "SimulcastEncoderAdapter" in outbound_rtp_r0["encoderImplementation"]
        assert "libvpl" in outbound_rtp_r0["encoderImplementation"]

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

        # Check the encoder implementation of r1.
        assert "encoderImplementation" in outbound_rtp_r1
        assert "SimulcastEncoderAdapter" in outbound_rtp_r1["encoderImplementation"]
        assert "libvpl" in outbound_rtp_r1["encoderImplementation"]

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

        # Check the encoder implementation of r2.
        assert "encoderImplementation" in outbound_rtp_r2
        assert "SimulcastEncoderAdapter" in outbound_rtp_r2["encoderImplementation"]
        assert "libvpl" in outbound_rtp_r2["encoderImplementation"]

        assert outbound_rtp_r2["frameWidth"] == 960
        assert outbound_rtp_r2["frameHeight"] == 528
        print(f"r2: {outbound_rtp_r2['frameWidth']}x{outbound_rtp_r2['frameHeight']}")

        # Get and confirm the transport.
        transport_stats = [stat for stat in stats if stat.get("type") == "transport"]
        assert len(transport_stats) == 1, f"Expected 1 transport, but got {len(transport_stats)}"

        # Check the details of the transport.
        transport = transport_stats[0]
        assert "bytesSent" in transport
        assert "bytesReceived" in transport
        assert "dtlsState" in transport
        assert "iceState" in transport
        assert transport["bytesSent"] > 0
        assert transport["bytesReceived"] > 0
        assert transport["dtlsState"] == "connected"
        assert transport["iceState"] == "connected"

        # Get and confirm the peer-connection.
        peer_connection_stats = [stat for stat in stats if stat.get("type") == "peer-connection"]
        assert len(peer_connection_stats) == 1, (
            f"Expected 1 peer-connection, but got {len(peer_connection_stats)}"
        )

        # Check the details of the peer-connection.
        peer_connection = peer_connection_stats[0]
        assert "dataChannelsOpened" in peer_connection


@pytest.mark.parametrize(
    "video_codec_type",
    [
        "VP9",
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
    """Confirm that sendonly and recvonly pair is created and sent and received in Sora mode (using Intel VPL)."""

    # Generate expected_mime_type.
    expected_mime_type = f"video/{video_codec_type}"

    # Prepare encoder settings.
    encoder_params = {}
    match video_codec_type:
        case "VP9":
            encoder_params["vp9_encoder"] = "vpl"
        case "AV1":
            encoder_params["av1_encoder"] = "vpl"
        case "H264":
            encoder_params["h264_encoder"] = "vpl"
        case "H265":
            encoder_params["h265_encoder"] = "vpl"

    # Prepare decoder settings.
    decoder_params = {}
    match video_codec_type:
        case "VP9":
            decoder_params["vp9_decoder"] = "vpl"
        case "AV1":
            decoder_params["av1_decoder"] = "vpl"
        case "H264":
            decoder_params["h264_decoder"] = "vpl"
        case "H265":
            decoder_params["h265_decoder"] = "vpl"

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
                        "encoderImplementation": "libvpl",
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
                        "decoderImplementation": "libvpl",
                    }
                ]
            )
            receiver_stats = receiver_data.get("stats", [])

            # Confirm that there are 2 outbound-rtp for audio and video on the sender.
            sender_outbound_rtp = [
                stat for stat in sender_stats if stat.get("type") == "outbound-rtp"
            ]
            assert len(sender_outbound_rtp) == 2, (
                "Sender should have exactly 2 outbound-rtp stats (audio and video)"
            )

            # Check the codec information of the sender (at least 2 for audio and video).
            sender_codecs = [stat for stat in sender_stats if stat.get("type") == "codec"]
            assert len(sender_codecs) >= 2, "Should have at least 2 codecs (audio and video)"

            # Confirm that the mimeType of the video codec is correct.
            sender_video_codec = next(
                (stat for stat in sender_codecs if stat.get("mimeType", "").startswith("video/")),
                None,
            )
            assert sender_video_codec is not None, "Video codec should be present"
            assert sender_video_codec["mimeType"] == expected_mime_type, (
                f"Expected {expected_mime_type}, got {sender_video_codec['mimeType']}"
            )

            # Confirm that the mimeType of the audio codec is correct.
            sender_audio_codec = next(
                (stat for stat in sender_codecs if stat.get("mimeType", "").startswith("audio/")),
                None,
            )
            assert sender_audio_codec is not None, "Audio codec should be present"
            assert sender_audio_codec["mimeType"] == "audio/opus", "Audio codec should be opus"

            # Get and confirm the audio outbound-rtp of the sender.
            sender_audio_outbound = next(
                (stat for stat in sender_outbound_rtp if stat.get("kind") == "audio"), None
            )
            assert sender_audio_outbound is not None, "Audio outbound-rtp should be present"
            assert "packetsSent" in sender_audio_outbound
            assert "bytesSent" in sender_audio_outbound
            assert sender_audio_outbound["packetsSent"] > 0
            assert sender_audio_outbound["bytesSent"] > 0

            # Get and confirm the video outbound-rtp of the sender.
            sender_video_outbound = next(
                (stat for stat in sender_outbound_rtp if stat.get("kind") == "video"), None
            )
            assert sender_video_outbound is not None, "Video outbound-rtp should be present"
            assert "packetsSent" in sender_video_outbound
            assert "bytesSent" in sender_video_outbound
            assert "encoderImplementation" in sender_video_outbound
            assert sender_video_outbound["packetsSent"] > 0
            assert sender_video_outbound["bytesSent"] > 0
            assert sender_video_outbound["encoderImplementation"] == "libvpl"

            # Confirm that there are 2 inbound-rtp for audio and video on the receiver.
            receiver_inbound_rtp = [
                stat for stat in receiver_stats if stat.get("type") == "inbound-rtp"
            ]
            assert len(receiver_inbound_rtp) == 2, (
                "Receiver should have exactly 2 inbound-rtp stats (audio and video)"
            )

            # Check the codec information of the receiver (at least 2 for audio and video).
            receiver_codecs = [stat for stat in receiver_stats if stat.get("type") == "codec"]
            assert len(receiver_codecs) >= 2, (
                "Should have at least 2 codecs (audio and video) on receiver"
            )

            # Confirm that the mimeType of the video codec is correct.
            receiver_video_codec = next(
                (stat for stat in receiver_codecs if stat.get("mimeType", "").startswith("video/")),
                None,
            )
            assert receiver_video_codec is not None, "Video codec should be present on receiver"
            assert receiver_video_codec["mimeType"] == expected_mime_type, (
                f"Expected {expected_mime_type}, got {receiver_video_codec['mimeType']} on receiver"
            )

            # Confirm that the mimeType of the audio codec is correct.
            receiver_audio_codec = next(
                (stat for stat in receiver_codecs if stat.get("mimeType", "").startswith("audio/")),
                None,
            )
            assert receiver_audio_codec is not None, "Audio codec should be present on receiver"
            assert receiver_audio_codec["mimeType"] == "audio/opus", (
                "Audio codec should be opus on receiver"
            )

            # Get and confirm the audio inbound-rtp of the receiver.
            receiver_audio_inbound = next(
                (stat for stat in receiver_inbound_rtp if stat.get("kind") == "audio"), None
            )
            assert receiver_audio_inbound is not None, "Audio inbound-rtp should be present"
            assert "packetsReceived" in receiver_audio_inbound
            assert "bytesReceived" in receiver_audio_inbound
            assert receiver_audio_inbound["packetsReceived"] > 0
            assert receiver_audio_inbound["bytesReceived"] > 0

            # Get and confirm the video inbound-rtp of the receiver.
            receiver_video_inbound = next(
                (stat for stat in receiver_inbound_rtp if stat.get("kind") == "video"), None
            )
            assert receiver_video_inbound is not None, "Video inbound-rtp should be present"
            assert "packetsReceived" in receiver_video_inbound
            assert "bytesReceived" in receiver_video_inbound
            assert "decoderImplementation" in receiver_video_inbound
            assert receiver_video_inbound["packetsReceived"] > 0
            assert receiver_video_inbound["bytesReceived"] > 0
            assert receiver_video_inbound["decoderImplementation"] == "libvpl"
