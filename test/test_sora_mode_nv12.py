import os

import pytest

from momo import Momo, MomoMode

# Skip Sora mode tests if TEST_SORA_MODE_SIGNALING_URLS is not set.
pytestmark = pytest.mark.skipif(
    not os.environ.get("TEST_SORA_MODE_SIGNALING_URLS"),
    reason="TEST_SORA_MODE_SIGNALING_URLS not set in environment",
)


def test_sendonly_with_force_nv12_outbound_rtp(sora_settings, free_port):
    """Confirm that send statistics can be obtained by specifying fake capture + --force-nv12 in Sora mode."""

    with Momo(
        mode=MomoMode.SORA,
        metrics_port=free_port,
        fake_capture_device=True,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        audio=True,
        video=True,
        force_nv12=True,
        metadata=sora_settings.metadata,
    ) as m:
        # Wait for the connection to be established.
        assert m.wait_for_connection(), "Failed to establish connection with --force-nv12"

        # Wait for the outbound-rtp(kind=video) to appear.
        data = m.get_metrics(
            wait_stats=[{"type": "outbound-rtp", "kind": "video"}],
            wait_stats_timeout=10,
        )
        stats = data.get("stats", [])

        # Confirm that the send statistics exists and the send is progressing.
        video_outbound = [
            s for s in stats if s.get("type") == "outbound-rtp" and s.get("kind") == "video"
        ]
        assert len(video_outbound) >= 1, "No video outbound-rtp stats found"
        v = video_outbound[0]
        assert v.get("packetsSent", 0) > 0
        assert v.get("bytesSent", 0) > 0
