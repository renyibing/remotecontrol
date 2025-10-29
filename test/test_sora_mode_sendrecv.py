import os

import pytest

from momo import Momo, MomoMode

# Testing Sora mode is skipped if TEST_SORA_MODE_SIGNALING_URLS is not set.
pytestmark = pytest.mark.skipif(
    not os.environ.get("TEST_SORA_MODE_SIGNALING_URLS"),
    reason="TEST_SORA_MODE_SIGNALING_URLS not set in environment",
)


@pytest.mark.parametrize(
    "video_codec_type, expected_encoder_implementation, expected_decoder_implementation",
    [
        ("VP8", "libvpx", "libvpx"),
        ("VP9", "libvpx", "libvpx"),
        ("AV1", "libaom", "dav1d"),
    ],
)
def test_sendrecv(
    sora_settings,
    port_allocator,
    video_codec_type,
    expected_encoder_implementation,
    expected_decoder_implementation,
):
    """Confirm that sendrecv bidirectional communication works in Sora mode."""

    # Generate expected_mime_type.
    expected_mime_type = f"video/{video_codec_type}"

    # Prepare encoder and decoder settings.
    encoder_params = {}
    decoder_params = {}
    match video_codec_type:
        case "VP8":
            encoder_params["vp8_encoder"] = "software"
            decoder_params["vp8_decoder"] = "software"
        case "VP9":
            encoder_params["vp9_encoder"] = "software"
            decoder_params["vp9_decoder"] = "software"
        case "AV1":
            encoder_params["av1_encoder"] = "software"
            decoder_params["av1_decoder"] = "software"

    # Prepare sendrecv client.
    with Momo(
        mode=MomoMode.SORA,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendrecv",
        metrics_port=next(port_allocator),
        fake_capture_device=True,
        video=True,
        video_codec_type=video_codec_type,
        audio=True,
        metadata=sora_settings.metadata,
        **encoder_params,
        **decoder_params,
    ) as client1:
        # Prepare sendrecv client.
        with Momo(
            mode=MomoMode.SORA,
            signaling_urls=sora_settings.signaling_urls,
            channel_id=sora_settings.channel_id,
            role="sendrecv",
            metrics_port=next(port_allocator),
            fake_capture_device=True,
            video=True,
            video_codec_type=video_codec_type,
            audio=True,
            metadata=sora_settings.metadata,
            **encoder_params,
            **decoder_params,
        ) as client2:
            # Wait for the connection to be established.
            assert client1.wait_for_connection(), (
                f"Client1 failed to establish connection for {video_codec_type}"
            )
            assert client2.wait_for_connection(), (
                f"Client2 failed to establish connection for {video_codec_type}"
            )

            # Confirm the statistics of the client1.
            client1_data = client1.get_metrics(
                wait_stats=[
                    {
                        "type": "outbound-rtp",
                        "encoderImplementation": expected_encoder_implementation,
                    },
                    {
                        "type": "inbound-rtp",
                        "decoderImplementation": expected_decoder_implementation,
                    },
                ]
            )
            client1_stats = client1_data.get("stats", [])

            # Confirm the statistics of the client2.
            client2_data = client2.get_metrics(
                wait_stats=[
                    {
                        "type": "outbound-rtp",
                        "encoderImplementation": expected_encoder_implementation,
                    },
                    {
                        "type": "inbound-rtp",
                        "decoderImplementation": expected_decoder_implementation,
                    },
                ]
            )
            client2_stats = client2_data.get("stats", [])

            # Confirm that outbound-rtp exists for audio and video on the client1.
            client1_outbound_rtp = [
                stat for stat in client1_stats if stat.get("type") == "outbound-rtp"
            ]
            assert len(client1_outbound_rtp) == 2, (
                "Client1 should have exactly 2 outbound-rtp stats (audio and video)"
            )

            # Confirm that inbound-rtp exists for audio and video on the client1.
            client1_inbound_rtp = [
                stat for stat in client1_stats if stat.get("type") == "inbound-rtp"
            ]
            assert len(client1_inbound_rtp) == 2, (
                "Client1 should have exactly 2 inbound-rtp stats (audio and video)"
            )

            # Confirm that the data is being sent on the client1.
            for stat in client1_outbound_rtp:
                assert "packetsSent" in stat
                assert "bytesSent" in stat
                assert stat["packetsSent"] > 0
                assert stat["bytesSent"] > 0

                # Confirm that the encoderImplementation of the video stream is libvpx.
                if stat.get("kind") == "video":
                    assert "encoderImplementation" in stat
                    assert stat["encoderImplementation"] == expected_encoder_implementation

            # Confirm that the data is being received on the client1.
            for stat in client1_inbound_rtp:
                assert "packetsReceived" in stat
                assert "bytesReceived" in stat
                assert stat["packetsReceived"] > 0
                assert stat["bytesReceived"] > 0

                # Confirm that the decoderImplementation of the video stream is libvpx.
                if stat.get("kind") == "video":
                    assert "decoderImplementation" in stat
                    assert stat["decoderImplementation"] == expected_decoder_implementation

            # Confirm that outbound-rtp exists for audio and video on the client2.
            client2_outbound_rtp = [
                stat for stat in client2_stats if stat.get("type") == "outbound-rtp"
            ]
            assert len(client2_outbound_rtp) == 2, (
                "Client2 should have exactly 2 outbound-rtp stats (audio and video)"
            )

            # Confirm that inbound-rtp exists for audio and video on the client2.
            client2_inbound_rtp = [
                stat for stat in client2_stats if stat.get("type") == "inbound-rtp"
            ]
            assert len(client2_inbound_rtp) == 2, (
                "Client2 should have exactly 2 inbound-rtp stats (audio and video)"
            )

            # Confirm that the data is being sent on the client2.
            for stat in client2_outbound_rtp:
                assert "packetsSent" in stat
                assert "bytesSent" in stat
                assert stat["packetsSent"] > 0
                assert stat["bytesSent"] > 0

                # Confirm that the encoderImplementation of the video stream is libvpx.
                if stat.get("kind") == "video":
                    assert "encoderImplementation" in stat
                    assert stat["encoderImplementation"] == expected_encoder_implementation

            # Confirm that the data is being received on the client2.
            for stat in client2_inbound_rtp:
                assert "packetsReceived" in stat
                assert "bytesReceived" in stat
                assert stat["packetsReceived"] > 0
                assert stat["bytesReceived"] > 0

                # Confirm that the decoderImplementation of the video stream is libvpx.
                if stat.get("kind") == "video":
                    assert "decoderImplementation" in stat
                    assert stat["decoderImplementation"] == expected_decoder_implementation

            # Confirm the codec information of both clients.
            for client_stats, client_name in [
                (client1_stats, "Client1"),
                (client2_stats, "Client2"),
            ]:
                codecs = [stat for stat in client_stats if stat.get("type") == "codec"]
                assert len(codecs) >= 2, (
                    f"{client_name} should have at least 2 codecs (audio and video)"
                )

                # Confirm the mimeType of the video codec.
                video_codec = next(
                    (stat for stat in codecs if stat.get("mimeType", "").startswith("video/")),
                    None,
                )
                assert video_codec is not None, f"Video codec should be present on {client_name}"
                assert video_codec["mimeType"] == expected_mime_type, (
                    f"Expected {expected_mime_type}, got {video_codec['mimeType']} on {client_name}"
                )

                # Confirm the mimeType of the audio codec.
                audio_codec = next(
                    (stat for stat in codecs if stat.get("mimeType", "").startswith("audio/")),
                    None,
                )
                assert audio_codec is not None, f"Audio codec should be present on {client_name}"
                assert audio_codec["mimeType"] == "audio/opus", (
                    f"Audio codec should be opus on {client_name}"
                )
