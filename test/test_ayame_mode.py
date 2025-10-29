"""E2E test for Ayame mode."""

import uuid
from typing import Any

import pytest

from momo import Momo, MomoMode

AYAME_SIGNALING_URL = "wss://ayame-labo.shiguredo.app/signaling"


def find_stats(metrics_data: dict[str, Any], **filters) -> dict[str, Any] | None:
    """Search for the first statistics that matches the specified conditions in the metrics data.

    Args:
        metrics_data: Metrics data obtained by get_metrics()
        **filters: Search conditions (e.g. type="outbound-rtp", kind="video")

    Returns:
        Statistics that matches the conditions, None if not found
    """
    stats = metrics_data.get("stats", [])
    return next(
        (stat for stat in stats if all(stat.get(key) == value for key, value in filters.items())),
        None,
    )


def find_all_stats(metrics_data: dict[str, Any], **filters) -> list[dict[str, Any]]:
    """Search for all statistics that match the specified conditions in the metrics data.

    Args:
        metrics_data: Metrics data obtained by get_metrics()
        **filters: Search conditions (e.g. type="codec")

    Returns:
        List of statistics that match the conditions
    """
    stats = metrics_data.get("stats", [])
    return [stat for stat in stats if all(stat.get(key) == value for key, value in filters.items())]


def test_ayame_mode_basic(free_port, port_allocator):
    """Confirm that momo can be started in Ayame mode."""
    room_id = str(uuid.uuid4())

    with Momo(
        mode=MomoMode.AYAME,
        ayame_signaling_url=AYAME_SIGNALING_URL,
        room_id=room_id,
        metrics_port=free_port,
        fake_capture_device=True,
        resolution="QVGA",
        framerate=30,
        log_level="info",
    ) as m:
        data = m.get_metrics()
        assert "version" in data


def test_ayame_mode_with_client_id(free_port, port_allocator):
    """Confirm that momo can be started with client_id in Ayame mode."""
    room_id = str(uuid.uuid4())
    client_id = str(uuid.uuid4())

    with Momo(
        mode=MomoMode.AYAME,
        ayame_signaling_url=AYAME_SIGNALING_URL,
        room_id=room_id,
        client_id=client_id,
        metrics_port=free_port,
        fake_capture_device=True,
        resolution="QVGA",
        framerate=15,
    ) as m:
        data = m.get_metrics()
        assert "version" in data  # Metrics can be obtained.


def test_ayame_mode_with_video_settings(free_port, port_allocator):
    """Confirm that momo can be started with video settings in Ayame mode."""
    room_id = str(uuid.uuid4())

    with Momo(
        mode=MomoMode.AYAME,
        ayame_signaling_url=AYAME_SIGNALING_URL,
        room_id=room_id,
        metrics_port=free_port,
        fake_capture_device=True,
        resolution="QVGA",
        framerate=15,
        vp8_encoder="software",
    ) as m:
        data = m.get_metrics()
        assert "version" in data  # Metrics can be obtained.


@pytest.mark.parametrize("codec", ["VP8", "VP9", "AV1"])
def test_ayame_mode_with_codec(port_allocator, codec):
    """Confirm that communication using various codecs in Ayame mode works."""
    room_id = str(uuid.uuid4())

    # Encoder/decoder settings for each codec.
    match codec:
        case "VP8":
            codec_settings = {
                "vp8_encoder": "software",
                "vp8_decoder": "software",
            }
        case "VP9":
            codec_settings = {
                "vp9_encoder": "software",
                "vp9_decoder": "software",
            }
        case "AV1":
            codec_settings = {
                "av1_encoder": "software",
                "av1_decoder": "software",
            }
        case _:
            codec_settings = {}

    with Momo(
        mode=MomoMode.AYAME,
        ayame_signaling_url=AYAME_SIGNALING_URL,
        room_id=room_id,
        client_id=str(uuid.uuid4()),
        metrics_port=next(port_allocator),
        fake_capture_device=True,
        resolution="QVGA",
        ayame_video_codec_type=codec,  # Specify the codec.
        **codec_settings,
    ) as m1:
        with Momo(
            mode=MomoMode.AYAME,
            ayame_signaling_url=AYAME_SIGNALING_URL,
            room_id=room_id,
            client_id=str(uuid.uuid4()),
            metrics_port=next(port_allocator),
            fake_capture_device=True,
            resolution="QVGA",
            ayame_video_codec_type=codec,  # Specify the codec.
            **codec_settings,
        ) as m2:
            # Wait for both peers to establish connections.
            assert m1.wait_for_connection(timeout=10), (
                f"M1 failed to establish connection for {codec} codec within timeout"
            )
            assert m2.wait_for_connection(timeout=10), (
                f"M2 failed to establish connection for {codec} codec within timeout"
            )

            p1_data = m1.get_metrics()
            p2_data = m2.get_metrics()

            # Check the codec in p1 outbound-rtp.
            assert (
                p1_outbound := find_stats(p1_data, type="outbound-rtp", kind="video")
            ) is not None, "Could not find p1 outbound-rtp video stream"

            assert (codec_id := p1_outbound.get("codecId")) is not None, (
                "No codecId found in p1 outbound-rtp stats"
            )

            # Search for codec statistics from codecId.
            assert (p1_codec := find_stats(p1_data, id=codec_id, type="codec")) is not None, (
                f"Could not find codec stats for codecId: {codec_id}"
            )

            mime_type = p1_codec.get("mimeType", "")
            # Remove "video/" and "audio/" from mimeType to get the codec name.
            codec_name = mime_type.split("/")[-1] if "/" in mime_type else mime_type
            assert codec == codec_name.upper(), f"Expected {codec} codec but got: {mime_type}"
            print(f"P1 codec for {codec}: {mime_type}")

            # Check the codec in p2 outbound-rtp (bidirectional communication).
            assert (
                p2_outbound := find_stats(p2_data, type="outbound-rtp", kind="video")
            ) is not None, "Could not find p2 outbound-rtp video stream"

            assert (codec_id := p2_outbound.get("codecId")) is not None, (
                "No codecId found in p2 outbound-rtp stats"
            )

            # Search for codec statistics from codecId.
            assert (p2_codec := find_stats(p2_data, id=codec_id, type="codec")) is not None, (
                f"Could not find codec stats for codecId: {codec_id}"
            )

            mime_type = p2_codec.get("mimeType", "")
            # Remove "video/" and "audio/" from mimeType to get the codec name.
            codec_name = mime_type.split("/")[-1] if "/" in mime_type else mime_type
            assert codec == codec_name.upper(), f"Expected {codec} codec but got: {mime_type}"
            print(f"P2 codec for {codec}: {mime_type}")


def test_ayame_mode_with_audio_settings(free_port, port_allocator):
    """Confirm that momo can be started with audio settings in Ayame mode."""
    room_id = str(uuid.uuid4())

    with Momo(
        mode=MomoMode.AYAME,
        ayame_signaling_url=AYAME_SIGNALING_URL,
        room_id=room_id,
        metrics_port=free_port,
        fake_capture_device=True,
        disable_echo_cancellation=True,
        disable_auto_gain_control=True,
        disable_noise_suppression=True,
    ) as m:
        data = m.get_metrics()
        assert "version" in data  # Metrics can be obtained.


def test_ayame_mode_peer_connection(port_allocator):
    """Confirm that two peers can communicate bidirectionally in Ayame mode."""
    room_id = str(uuid.uuid4())

    with Momo(
        mode=MomoMode.AYAME,
        ayame_signaling_url=AYAME_SIGNALING_URL,
        room_id=room_id,
        client_id=str(uuid.uuid4()),
        metrics_port=next(port_allocator),
        fake_capture_device=True,
        resolution="QVGA",
        initial_wait=10,
    ) as m1:
        with Momo(
            mode=MomoMode.AYAME,
            ayame_signaling_url=AYAME_SIGNALING_URL,
            room_id=room_id,
            client_id=str(uuid.uuid4()),
            metrics_port=next(port_allocator),
            fake_capture_device=True,
            resolution="QVGA",
            initial_wait=10,
        ) as m2:
            # Wait for both peers to establish connections.
            assert m1.wait_for_connection(timeout=10), (
                "M1 failed to establish connection within timeout"
            )
            assert m2.wait_for_connection(timeout=10), (
                "M2 failed to establish connection within timeout"
            )

            p1_data = m1.get_metrics()
            p2_data = m2.get_metrics()

            # wait_for_connection has succeeded, so stats should exist.
            # Check the send/receive in p1 (both send and receive should exist).
            assert (
                p1_video_out := find_stats(p1_data, type="outbound-rtp", kind="video")
            ) is not None, "P1 should have video outbound-rtp"
            assert (
                p1_video_in := find_stats(p1_data, type="inbound-rtp", kind="video")
            ) is not None, "P1 should have video inbound-rtp"
            assert p1_video_out.get("packetsSent", 0) > 0
            assert p1_video_in.get("packetsReceived", 0) > 0

            # Check the send/receive in p2 (both send and receive should exist).
            assert (
                p2_video_out := find_stats(p2_data, type="outbound-rtp", kind="video")
            ) is not None, "P2 should have video outbound-rtp"
            assert (
                p2_video_in := find_stats(p2_data, type="inbound-rtp", kind="video")
            ) is not None, "P2 should have video inbound-rtp"
            assert p2_video_out.get("packetsSent", 0) > 0
            assert p2_video_in.get("packetsReceived", 0) > 0

            print(
                f"P1 video - sent: {p1_video_out.get('packetsSent')}, received: {p1_video_in.get('packetsReceived')}"
            )
            print(
                f"P2 video - sent: {p2_video_out.get('packetsSent')}, received: {p2_video_in.get('packetsReceived')}"
            )


def test_ayame_mode_direction_sendonly_recvonly(port_allocator):
    """Confirm that communication between sendonly and recvonly pairs using direction in Ayame mode works."""
    room_id = str(uuid.uuid4())

    # sendonly side
    with Momo(
        mode=MomoMode.AYAME,
        ayame_signaling_url=AYAME_SIGNALING_URL,
        room_id=room_id,
        client_id=str(uuid.uuid4()),
        direction="sendonly",
        metrics_port=next(port_allocator),
        fake_capture_device=True,
        resolution="QVGA",
    ) as sender:
        # recvonly side
        with Momo(
            mode=MomoMode.AYAME,
            ayame_signaling_url=AYAME_SIGNALING_URL,
            room_id=room_id,
            client_id=str(uuid.uuid4()),
            direction="recvonly",
            metrics_port=next(port_allocator),
            resolution="QVGA",
        ) as receiver:
            # Wait for both peers to establish connections.
            assert sender.wait_for_connection(timeout=10), (
                "Sender failed to establish connection within timeout"
            )
            assert receiver.wait_for_connection(timeout=10), (
                "Receiver failed to establish connection within timeout"
            )

            sender_data = sender.get_metrics()
            receiver_data = receiver.get_metrics()

            # sendonly only sends (outbound-rtp exists but inbound-rtp does not exist).
            assert (
                sender_video_out := find_stats(sender_data, type="outbound-rtp", kind="video")
            ) is not None, "Sender should have video outbound-rtp"
            assert sender_video_out.get("packetsSent", 0) > 0

            # sendonly does not receive.
            assert (find_stats(sender_data, type="inbound-rtp", kind="video")) is None, (
                "Sender should NOT have video inbound-rtp"
            )

            # recvonly only receives (inbound-rtp exists but outbound-rtp does not exist).
            assert (
                receiver_video_in := find_stats(receiver_data, type="inbound-rtp", kind="video")
            ) is not None, "Receiver should have video inbound-rtp"
            assert receiver_video_in.get("packetsReceived", 0) > 0

            # recvonly does not send.
            assert (find_stats(receiver_data, type="outbound-rtp", kind="video")) is None, (
                "Receiver should NOT have video outbound-rtp"
            )

            print(f"Sender video - sent: {sender_video_out.get('packetsSent')}")
            print(f"Receiver video - received: {receiver_video_in.get('packetsReceived')}")


def test_ayame_mode_direction_sendrecv_default(free_port):
    """Confirm that direction defaults to sendrecv in Ayame mode."""
    room_id = str(uuid.uuid4())

    # direction is not specified (default = sendrecv).
    with Momo(
        mode=MomoMode.AYAME,
        ayame_signaling_url=AYAME_SIGNALING_URL,
        room_id=room_id,
        metrics_port=free_port,
        fake_capture_device=True,
        resolution="QVGA",
    ) as m:
        # Explicitly specify sendrecv.
        with Momo(
            mode=MomoMode.AYAME,
            ayame_signaling_url=AYAME_SIGNALING_URL,
            room_id=room_id,
            client_id=str(uuid.uuid4()),
            direction="sendrecv",
            metrics_port=free_port + 1,
            fake_capture_device=True,
            resolution="QVGA",
        ) as m2:
            # Wait for both peers to establish connections.
            assert m.wait_for_connection(timeout=10), (
                "M1 failed to establish connection within timeout"
            )
            assert m2.wait_for_connection(timeout=10), (
                "M2 failed to establish connection within timeout"
            )

            m1_data = m.get_metrics()
            m2_data = m2.get_metrics()

            # Both should be able to send and receive.
            assert find_stats(m1_data, type="outbound-rtp", kind="video") is not None
            assert find_stats(m1_data, type="inbound-rtp", kind="video") is not None
            assert find_stats(m2_data, type="outbound-rtp", kind="video") is not None
            assert find_stats(m2_data, type="inbound-rtp", kind="video") is not None


def test_ayame_mode_with_invalid_codec(port_allocator):
    """Confirm that an error occurs when an invalid codec is specified."""
    room_id = str(uuid.uuid4())

    # Specify an invalid video codec.
    # To suppress type checker warnings, use type: ignore comment.
    with pytest.raises(RuntimeError, match="momo process exited unexpectedly"):
        with Momo(
            mode=MomoMode.AYAME,
            ayame_signaling_url=AYAME_SIGNALING_URL,
            room_id=room_id,
            metrics_port=next(port_allocator),
            fake_capture_device=True,
            ayame_video_codec_type="INVALID_CODEC",  # type: ignore[arg-type] Invalid codec
        ):
            pass  # This should not be reached.

    # Specify an invalid audio codec.
    with pytest.raises(RuntimeError, match="momo process exited unexpectedly"):
        with Momo(
            mode=MomoMode.AYAME,
            ayame_signaling_url=AYAME_SIGNALING_URL,
            room_id=room_id,
            metrics_port=next(port_allocator),
            fake_capture_device=True,
            ayame_audio_codec_type="INVALID_AUDIO",  # type: ignore[arg-type] Invalid codec
        ):
            pass  # This should not be reached.
