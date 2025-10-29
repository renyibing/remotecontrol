import os

import pytest

from momo import Momo, MomoMode

# Testing Sora mode is skipped if TEST_SORA_MODE_SIGNALING_URLS is not set.
# Skipping simulcast test on macOS due to performance limitations.
pytestmark = [
    pytest.mark.skipif(
        not os.environ.get("TEST_SORA_MODE_SIGNALING_URLS"),
        reason="TEST_SORA_MODE_SIGNALING_URLS not set in environment",
    ),
    # pytest.mark.skipif(
    #     platform.system() == "Darwin",
    #     reason="Skipping simulcast test on macOS due to performance limitations",
    # ),
]


@pytest.mark.parametrize(
    "video_codec_type,expected_encoder_implementation",
    [
        ("VP8", "SimulcastEncoderAdapter (libvpx, libvpx, libvpx)"),
        ("VP9", "SimulcastEncoderAdapter (libvpx, libvpx, libvpx)"),
        ("AV1", "SimulcastEncoderAdapter (libaom, libaom, libaom)"),
    ],
)
def test_simulcast(sora_settings, video_codec_type, expected_encoder_implementation, free_port):
    """Confirm that connection statistics are collected in Sora mode when simulcast is connected."""
    # Debug: Output test case information.
    print(f"\n=== Running test for {video_codec_type} ===")
    print(f"Expected encoder implementation: {expected_encoder_implementation}")
    print(f"Free port: {free_port}")

    # Prepare encoder settings.
    encoder_params = {}
    if video_codec_type == "VP8":
        encoder_params["vp8_encoder"] = "software"
        print(f"Setting VP8 encoder parameter: {encoder_params}")
    elif video_codec_type == "VP9":
        encoder_params["vp9_encoder"] = "software"
        print(f"Setting VP9 encoder parameter: {encoder_params}")
    elif video_codec_type == "AV1":
        encoder_params["av1_encoder"] = "software"
        print(f"Setting AV1 encoder parameter: {encoder_params}")

    print(f"Final encoder_params: {encoder_params}")

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
        framerate=15,  # 15 fps
        video_bit_rate=3000,  # 3000 bit rate
        metadata=sora_settings.metadata,
        **encoder_params,
    ) as m:
        # Wait for the connection to be established.
        assert m.wait_for_connection(), (
            f"Failed to establish simulcast connection for {video_codec_type}"
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
            wait_after_stats=10,
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

        # Confirm the encoder implementation of r0.
        assert "encoderImplementation" in outbound_rtp_r0
        assert outbound_rtp_r0["encoderImplementation"] == expected_encoder_implementation, (
            f"Expected encoder implementation {expected_encoder_implementation} for r0, "
            f"but got {outbound_rtp_r0['encoderImplementation']}"
        )

        # Confirm the resolution of r0.
        assert outbound_rtp_r0["frameWidth"] == 240
        assert outbound_rtp_r0["frameHeight"] == 128
        print(f"r0: {outbound_rtp_r0['frameWidth']}x{outbound_rtp_r0['frameHeight']}")

        # Output the qualityLimitationDurations of r0.
        if "qualityLimitationDurations" in outbound_rtp_r0:
            print("r0 qualityLimitationDurations:")
            for reason, duration in outbound_rtp_r0["qualityLimitationDurations"].items():
                print(f"  {reason}: {duration}")

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

        # Confirm the encoder implementation of r1.
        assert "encoderImplementation" in outbound_rtp_r1
        assert outbound_rtp_r1["encoderImplementation"] == expected_encoder_implementation, (
            f"Expected encoder implementation {expected_encoder_implementation} for r1, "
            f"but got {outbound_rtp_r1['encoderImplementation']}"
        )

        # Confirm the resolution of r1.
        assert outbound_rtp_r1["frameWidth"] == 480
        assert outbound_rtp_r1["frameHeight"] == 256
        print(f"r1: {outbound_rtp_r1['frameWidth']}x{outbound_rtp_r1['frameHeight']}")

        # Output the qualityLimitationDurations of r1.
        if "qualityLimitationDurations" in outbound_rtp_r1:
            print("r1 qualityLimitationDurations:")
            for reason, duration in outbound_rtp_r1["qualityLimitationDurations"].items():
                print(f"  {reason}: {duration}")

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

        # Confirm the encoder implementation of r2.
        assert "encoderImplementation" in outbound_rtp_r2
        assert outbound_rtp_r2["encoderImplementation"] == expected_encoder_implementation, (
            f"Expected encoder implementation {expected_encoder_implementation} for r2, "
            f"but got {outbound_rtp_r2['encoderImplementation']}"
        )

        # Confirm the resolution of r2.
        assert outbound_rtp_r2["frameWidth"] == 960
        assert outbound_rtp_r2["frameHeight"] == 528
        print(f"r2: {outbound_rtp_r2['frameWidth']}x{outbound_rtp_r2['frameHeight']}")

        # Output the qualityLimitationDurations of r2.
        if "qualityLimitationDurations" in outbound_rtp_r2:
            print("r2 qualityLimitationDurations:")
            for reason, duration in outbound_rtp_r2["qualityLimitationDurations"].items():
                print(f"  {reason}: {duration}")

        # Confirm the relationship between packets and bytes (r0 < r1 < r2).
        assert outbound_rtp_r0["bytesSent"] < outbound_rtp_r1["bytesSent"], (
            f"Expected r0 bytesSent ({outbound_rtp_r0['bytesSent']}) < r1 bytesSent ({outbound_rtp_r1['bytesSent']})"
        )
        assert outbound_rtp_r1["bytesSent"] < outbound_rtp_r2["bytesSent"], (
            f"Expected r1 bytesSent ({outbound_rtp_r1['bytesSent']}) < r2 bytesSent ({outbound_rtp_r2['bytesSent']})"
        )

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
