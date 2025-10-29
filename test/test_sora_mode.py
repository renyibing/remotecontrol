import os

import pytest

from momo import Momo, MomoMode

# Testing Sora mode is skipped if TEST_SORA_MODE_SIGNALING_URLS is not set.
pytestmark = pytest.mark.skipif(
    not os.environ.get("TEST_SORA_MODE_SIGNALING_URLS"),
    reason="TEST_SORA_MODE_SIGNALING_URLS not set in environment",
)


def test_metrics_endpoint_returns_200(sora_settings, free_port):
    """Confirm that the metrics endpoint returns 200 in Sora mode."""

    with Momo(
        mode=MomoMode.SORA,
        metrics_port=free_port,
        fake_capture_device=True,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        audio=True,
        video=True,
        metadata=sora_settings.metadata,
    ) as m:
        data = m.get_metrics()
        assert data is not None  # Confirm that the metrics are obtained.


def test_metrics_endpoint_response_structure(sora_settings, free_port):
    """Confirm the structure of the metrics response in Sora mode."""

    with Momo(
        mode=MomoMode.SORA,
        metrics_port=free_port,
        fake_capture_device=True,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        audio=True,
        video=True,
        metadata=sora_settings.metadata,
    ) as m:
        data = m.get_metrics()

        # Confirm the required fields.
        assert "version" in data
        assert "libwebrtc" in data
        assert "environment" in data
        assert "stats" in data

        # Confirm that the version information is a string.
        assert isinstance(data["version"], str)
        assert isinstance(data["libwebrtc"], str)
        assert isinstance(data["environment"], str)


def test_invalid_metrics_endpoint_returns_404(sora_settings, free_port):
    """Confirm that the invalid metrics endpoint returns 404 in Sora mode."""

    with Momo(
        mode=MomoMode.SORA,
        metrics_port=free_port,
        fake_capture_device=True,
        signaling_urls=sora_settings.signaling_urls,
        channel_id=sora_settings.channel_id,
        role="sendonly",
        audio=True,
        video=True,
        metadata=sora_settings.metadata,
    ) as m:
        response = m._http_client.get(f"http://localhost:{m.metrics_port}/invalid")
        assert response.status_code == 404
