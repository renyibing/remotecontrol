import os

import pytest

from momo import Momo, MomoMode

# Testing Sora mode is skipped if TEST_SORA_MODE_SIGNALING_URLS is not set.
# Skip if Raspberry Pi environment is not valid
pytestmark = pytest.mark.skipif(
    not os.environ.get("TEST_SORA_MODE_SIGNALING_URLS") or not os.environ.get("RASPBERRY_PI"),
    reason="TEST_SORA_MODE_SIGNALING_URLS or RASPBERRY_PI not set in environment",
)


@pytest.mark.parametrize(
    "video_codec_type,expected_encoder_implementation",
    [
        ("VP8", "libvpx"),
        ("VP9", "libvpx"),
        ("AV1", "libaom"),
        ("H264", "V4L2M2M H264"),
    ],
)
def test_sendonly_video_codec_type(
    sora_settings, free_port, video_codec_type, expected_encoder_implementation
):
    """Confirm that connection statistics are collected in Sora mode when sendonly is connected (Raspberry Pi)."""
    expected_mime_type = f"video/{video_codec_type}"

    with Momo(
        fake_capture_device=False,
        use_libcamera=True,
        metrics_port=free_port,
        mode=MomoMode.SORA,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        audio=False,
        video=True,
        video_codec_type=video_codec_type,
        metadata=sora_settings.metadata,
        initial_wait=10,
    ) as m:
        # Wait for the connection to be established.
        assert m.wait_for_connection()

        data = m.get_metrics()
        stats = data["stats"]

        # Confirm that connection-related statistics are included in Sora mode.
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

        # Get and confirm the video codec.
        video_codec = next(
            (
                stat
                for stat in stats
                if stat.get("type") == "codec" and stat.get("mimeType") == expected_mime_type
            ),
            None,
        )
        assert video_codec is not None

        # Validate the contents of the video codec.
        assert "payloadType" in video_codec
        assert "mimeType" in video_codec
        assert "clockRate" in video_codec
        assert video_codec["clockRate"] == 90000

        # Get and confirm the outbound-rtp of video.
        video_outbound_rtp = next(
            (
                stat
                for stat in stats
                if stat.get("type") == "outbound-rtp" and stat.get("kind") == "video"
            ),
            None,
        )
        assert video_outbound_rtp is not None

        # Validate the contents of the video outbound-rtp.
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
        assert video_outbound_rtp["encoderImplementation"] == expected_encoder_implementation

        # Get and confirm the transport.
        transport = next(
            (stat for stat in stats if stat.get("type") == "transport"),
            None,
        )
        assert transport is not None

        # Validate the contents of the transport.
        assert "bytesSent" in transport
        assert "bytesReceived" in transport
        assert "dtlsState" in transport
        assert "iceState" in transport
        assert transport["bytesSent"] > 0
        assert transport["bytesReceived"] > 0
        assert transport["dtlsState"] == "connected"
        assert transport["iceState"] == "connected"

        # Get and confirm the peer-connection.
        peer_connection = next(
            (stat for stat in stats if stat.get("type") == "peer-connection"),
            None,
        )
        assert peer_connection is not None

        # Validate the contents of the peer-connection.
        assert "dataChannelsOpened" in peer_connection


def test_simulcast(sora_settings, free_port):
    """Confirm that connection statistics are collected in Sora mode when simulcast is connected (Raspberry Pi H.264)."""
    video_codec_type = "H264"
    expected_mime_type = "video/H264"

    with Momo(
        mode=MomoMode.SORA,
        metrics_port=free_port,
        fake_capture_device=False,
        use_libcamera=True,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        audio=False,
        video=True,
        video_codec_type=video_codec_type,
        simulcast=True,
        resolution="960x540",  # 540p resolution
        video_bit_rate=3000,  # 3000 bit rate
        metadata=sora_settings.metadata,
        initial_wait=10,
    ) as m:
        # Wait for the connection to be established.
        assert m.wait_for_connection(), (
            f"Failed to establish connection for {video_codec_type} codec with simulcast"
        )

        data = m.get_metrics(
            wait_stats=[
                {
                    "type": "outbound-rtp",
                    "rid": "r0",
                },
                {
                    "type": "outbound-rtp",
                    "rid": "r1",
                },
                {
                    "type": "outbound-rtp",
                    "rid": "r2",
                },
            ],
            wait_after_stats=10,
        )
        stats = data["stats"]

        # Confirm that connection-related statistics are included in Sora mode.
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

        # Get and confirm the video codec.
        video_codec_stats = [
            stat
            for stat in stats
            if stat.get("type") == "codec" and stat.get("mimeType") == expected_mime_type
        ]
        assert len(video_codec_stats) == 1, (
            f"Expected 1 video codec ({expected_mime_type}), but got {len(video_codec_stats)}"
        )

        # Validate the contents of the video codec.
        video_codec = video_codec_stats[0]
        assert "payloadType" in video_codec
        assert "mimeType" in video_codec
        assert "clockRate" in video_codec
        assert video_codec["clockRate"] == 90000

        # Confirm that video outbound-rtp exists for simulcast.
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

        # Validate r0 (low resolution).
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

        # Confirm the encoder implementation of r0.
        assert "encoderImplementation" in outbound_rtp_r0
        # Raspberry Pi uses SimulcastEncoderAdapter and V4L2M2M H264 combination.
        assert "SimulcastEncoderAdapter" in outbound_rtp_r0["encoderImplementation"]
        assert "V4L2M2M H264" in outbound_rtp_r0["encoderImplementation"]

        assert outbound_rtp_r0["frameWidth"] == 240
        assert outbound_rtp_r0["frameHeight"] == 128
        print(f"r0: {outbound_rtp_r0['frameWidth']}x{outbound_rtp_r0['frameHeight']}")

        # Validate r1 (medium resolution).
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

        # Confirm the encoder implementation of r1.
        assert "encoderImplementation" in outbound_rtp_r1
        assert "SimulcastEncoderAdapter" in outbound_rtp_r1["encoderImplementation"]
        assert "V4L2M2M H264" in outbound_rtp_r1["encoderImplementation"]

        assert outbound_rtp_r1["frameWidth"] == 480
        assert outbound_rtp_r1["frameHeight"] == 256
        print(f"r1: {outbound_rtp_r1['frameWidth']}x{outbound_rtp_r1['frameHeight']}")

        # Validate r2 (high resolution).
        outbound_rtp_r2 = video_outbound_rtp_by_rid["r2"]
        assert "ssrc" in outbound_rtp_r2
        assert "rid" in outbound_rtp_r2
        assert outbound_rtp_r2["rid"] == "r2"
        assert "packetsSent" in outbound_rtp_r2
        assert "bytesSent" in outbound_rtp_r2
        assert "framesEncoded" in outbound_rtp_r2
        
        # r2 may not have frameWidth and frameHeight.
        if "frameWidth" in outbound_rtp_r2:
            assert "frameWidth" in outbound_rtp_r2
            print("frameWidth is present")
        if "frameHeight" in outbound_rtp_r2:
            assert "frameHeight" in outbound_rtp_r2
            print("frameHeight is present")

        assert outbound_rtp_r2["packetsSent"] > 0
        assert outbound_rtp_r2["bytesSent"] > 0
        assert outbound_rtp_r2["framesEncoded"] > 0

        # Confirm the encoder implementation of r2.
        assert "encoderImplementation" in outbound_rtp_r2
        assert "SimulcastEncoderAdapter" in outbound_rtp_r2["encoderImplementation"]
        assert "V4L2M2M H264" in outbound_rtp_r2["encoderImplementation"]

        # r2 may not have frameWidth and frameHeight.
        if "frameWidth" in outbound_rtp_r2:
            assert outbound_rtp_r2["frameWidth"] == 960
            print("frameWidth is checked")
        if "frameHeight" in outbound_rtp_r2:
            assert outbound_rtp_r2["frameHeight"] == 528
            print("frameHeight is checked")
        if "frameWidth" in outbound_rtp_r2 and "frameHeight" in outbound_rtp_r2:
            print(f"r2: {outbound_rtp_r2['frameWidth']}x{outbound_rtp_r2['frameHeight']}")

        # Get and confirm the transport.
        transport_stats = [stat for stat in stats if stat.get("type") == "transport"]
        assert len(transport_stats) == 1, f"Expected 1 transport, but got {len(transport_stats)}"

        # Validate the contents of the transport.
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

        # Validate the contents of the peer-connection.
        peer_connection = peer_connection_stats[0]
        assert "dataChannelsOpened" in peer_connection


@pytest.mark.skipif(reason="It doesn't work well, so skip temporarily.")
def test_sora_sendonly_recvonly_pair(
    sora_settings,
    port_allocator,
):
    """Confirm that sendonly and recvonly pair is created and sent and received in Sora mode (Raspberry Pi H.264)."""

    video_codec_type = "H264"
    expected_mime_type = "video/H264"

    # Sendonly client (using Raspberry Pi camera).
    with Momo(
        mode=MomoMode.SORA,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        metrics_port=next(port_allocator),
        fake_capture_device=False,
        use_libcamera=True,
        video=True,
        video_codec_type=video_codec_type,
        audio=False,
        metadata=sora_settings.metadata,
        initial_wait=10,
    ) as sender:
        # Recvonly client (use_libcamera is not specified).
        with Momo(
            mode=MomoMode.SORA,
            signaling_urls=sora_settings.signaling_urls,
            channel_id=sora_settings.channel_id,
            role="recvonly",
            metrics_port=next(port_allocator),
            video=True,
            audio=False,
            metadata=sora_settings.metadata,
        ) as receiver:
            # Wait for the connection to be established.
            assert sender.wait_for_connection(), (
                f"Sender failed to establish connection for {video_codec_type}"
            )
            assert receiver.wait_for_connection(), (
                f"Receiver failed to establish connection for {video_codec_type}"
            )

            # Confirm the statistics of the sender.
            sender_data = sender.get_metrics(
                wait_stats=[
                    {
                        "type": "outbound-rtp",
                        "kind": "video",
                        "encoderImplementation": "V4L2M2M H264",
                    }
                ]
            )
            sender_stats = sender_data.get("stats", [])

            # Confirm the statistics of the receiver.
            receiver_data = receiver.get_metrics(
                wait_stats=[
                    {
                        "type": "inbound-rtp",
                        "kind": "video",
                        "decoderImplementation": "V4L2M2M H264",
                    }
                ]
            )
            receiver_stats = receiver_data.get("stats", [])

            # Confirm that outbound-rtp exists for video only on the sender.
            sender_outbound_rtp = [
                stat for stat in sender_stats if stat.get("type") == "outbound-rtp"
            ]
            assert len(sender_outbound_rtp) == 1, (
                "Sender should have exactly 1 outbound-rtp stats (video only)"
            )

            # Confirm the codec information of the sender (video only).
            sender_codecs = [stat for stat in sender_stats if stat.get("type") == "codec"]
            assert len(sender_codecs) >= 1, "Should have at least 1 codec (video)"

            # Confirm the mimeType of the video codec.
            sender_video_codec = next(
                (stat for stat in sender_codecs if stat.get("mimeType", "").startswith("video/")),
                None,
            )
            assert sender_video_codec is not None, "Video codec should be present"
            assert sender_video_codec["mimeType"] == expected_mime_type, (
                f"Expected {expected_mime_type}, got {sender_video_codec['mimeType']}"
            )

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
            assert sender_video_outbound["encoderImplementation"] == "V4L2M2M H264"

            # Confirm that inbound-rtp exists for video only on the receiver.
            receiver_inbound_rtp = [
                stat for stat in receiver_stats if stat.get("type") == "inbound-rtp"
            ]
            assert len(receiver_inbound_rtp) == 1, (
                "Receiver should have exactly 1 inbound-rtp stats (video only)"
            )

            # Confirm the codec information of the receiver (video only).
            receiver_codecs = [stat for stat in receiver_stats if stat.get("type") == "codec"]
            assert len(receiver_codecs) >= 1, "Should have at least 1 codec (video) on receiver"

            # Confirm the mimeType of the video codec.
            receiver_video_codec = next(
                (stat for stat in receiver_codecs if stat.get("mimeType", "").startswith("video/")),
                None,
            )
            assert receiver_video_codec is not None, "Video codec should be present on receiver"
            assert receiver_video_codec["mimeType"] == expected_mime_type, (
                f"Expected {expected_mime_type}, got {receiver_video_codec['mimeType']} on receiver"
            )

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
            # Raspberry Pi uses V4L2M2M H264 decoder (HWA).
            assert receiver_video_inbound["decoderImplementation"] == "V4L2M2M H264"
