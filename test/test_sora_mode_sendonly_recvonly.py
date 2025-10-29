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
def test_sendonly_recvonly_pair(
    sora_settings,
    port_allocator,
    video_codec_type,
    expected_encoder_implementation,
    expected_decoder_implementation,
):
    """Confirm that sendonly and recvonly pair is created and sent and received in Sora mode."""

    print(sora_settings.channel_id)

    # Generate expected_mime_type.
    expected_mime_type = f"video/{video_codec_type}"

    # Prepare encoder settings.
    encoder_params = {}
    if video_codec_type == "VP8":
        encoder_params["vp8_encoder"] = "software"
    elif video_codec_type == "VP9":
        encoder_params["vp9_encoder"] = "software"
    elif video_codec_type == "AV1":
        encoder_params["av1_encoder"] = "software"

    # Prepare decoder settings.
    decoder_params = {}
    if video_codec_type == "VP8":
        decoder_params["vp8_decoder"] = "software"
    elif video_codec_type == "VP9":
        decoder_params["vp9_decoder"] = "software"
    elif video_codec_type == "AV1":
        decoder_params["av1_decoder"] = "software"

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

            # Confirm the statistics of the sender.
            sender_data = sender.get_metrics(
                wait_stats=[
                    {
                        "type": "outbound-rtp",
                        "encoderImplementation": expected_encoder_implementation,
                    }
                ]
            )
            sender_stats = sender_data.get("stats", [])

            # Confirm the statistics of the receiver.
            receiver_data = receiver.get_metrics(
                wait_stats=[
                    {
                        "type": "inbound-rtp",
                        "decoderImplementation": expected_decoder_implementation,
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

            # Confirm the codec information of the sender (at least 2 for audio and video).
            sender_codecs = [stat for stat in sender_stats if stat.get("type") == "codec"]
            assert len(sender_codecs) >= 2, "Should have at least 2 codecs (audio and video)"

            # Confirm the mimeType of the video codec.
            sender_video_codec = next(
                (stat for stat in sender_codecs if stat.get("mimeType", "").startswith("video/")),
                None,
            )
            assert sender_video_codec is not None, "Video codec should be present"
            assert sender_video_codec["mimeType"] == expected_mime_type, (
                f"Expected {expected_mime_type}, got {sender_video_codec['mimeType']}"
            )

            # Confirm the mimeType of the audio codec.
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

                # Confirm that the encoderImplementation of the video stream is libvpx.
                if stat.get("kind") == "video":
                    assert "encoderImplementation" in stat
                    assert stat["encoderImplementation"] == expected_encoder_implementation

            # Confirm that inbound-rtp exists for audio and video on the receiver.
            receiver_inbound_rtp = [
                stat for stat in receiver_stats if stat.get("type") == "inbound-rtp"
            ]
            assert len(receiver_inbound_rtp) == 2, (
                "Receiver should have exactly 2 inbound-rtp stats (audio and video)"
            )

            # Confirm the codec information of the receiver (at least 2 for audio and video).
            receiver_codecs = [stat for stat in receiver_stats if stat.get("type") == "codec"]
            assert len(receiver_codecs) >= 2, (
                "Should have at least 2 codecs (audio and video) on receiver"
            )

            # Confirm the mimeType of the video codec.
            receiver_video_codec = next(
                (stat for stat in receiver_codecs if stat.get("mimeType", "").startswith("video/")),
                None,
            )
            assert receiver_video_codec is not None, "Video codec should be present on receiver"
            assert receiver_video_codec["mimeType"] == expected_mime_type, (
                f"Expected {expected_mime_type}, got {receiver_video_codec['mimeType']} on receiver"
            )

            # Confirm the mimeType of the audio codec.
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

                # Confirm that the decoderImplementation of the video stream is libvpx.
                if stat.get("kind") == "video":
                    assert "decoderImplementation" in stat
                    assert stat["decoderImplementation"] == expected_decoder_implementation


def test_multiple_sendonly_clients(sora_settings, port_allocator):
    """Confirm that multiple sendonly clients can connect to the same channel."""

    # Launch multiple sendonly clients.
    with Momo(
        mode=MomoMode.SORA,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        metrics_port=next(port_allocator),
        fake_capture_device=True,
        video=True,
        audio=True,
        metadata=sora_settings.metadata,
    ) as sender1:
        with Momo(
            mode=MomoMode.SORA,
            signaling_urls=sora_settings.signaling_urls,
            channel_id=sora_settings.channel_id,
            role="sendonly",
            metrics_port=next(port_allocator),
            fake_capture_device=True,
            video=True,
            audio=True,
            metadata=sora_settings.metadata,
        ) as sender2:
            # Confirm that both instances are working correctly.
            metrics1 = sender1.get_metrics()
            assert metrics1 is not None

            metrics2 = sender2.get_metrics()
            assert metrics2 is not None
