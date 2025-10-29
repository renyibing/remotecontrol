"""E2E test for the metrics API."""

from momo import Momo, MomoMode


def test_metrics_response_structure(free_port, port_allocator):
    """Confirm the JSON structure of the metrics response."""
    with Momo(
        mode=MomoMode.P2P,
        metrics_port=free_port,
        port=next(port_allocator),
        fake_capture_device=True,
    ) as m:
        # get_metrics() calls raise_for_status() internally, so an exception is raised if the status code is not 200.
        data = m.get_metrics()
        assert data is not None
        assert isinstance(data, dict)

        # Check the required fields.
        assert "version" in data
        assert "libwebrtc" in data
        assert "environment" in data
        assert "stats" in data

        # Check that the version information is a string.
        assert isinstance(data["version"], str)
        assert isinstance(data["libwebrtc"], str)
        assert isinstance(data["environment"], str)

        # Check that the stats field exists (it may be an empty array in the initial state).
        assert data["stats"] is not None


def test_invalid_endpoint_returns_404(free_port, port_allocator):
    """Confirm that a non-existent endpoint returns 404."""
    with Momo(
        mode=MomoMode.P2P,
        metrics_port=free_port,
        port=next(port_allocator),
        fake_capture_device=True,
    ) as m:
        response = m._http_client.get(f"http://localhost:{m.metrics_port}/invalid")
        assert response.status_code == 404


def test_post_method_returns_error(free_port, port_allocator):
    """Confirm that the POST method returns an error."""
    with Momo(
        mode=MomoMode.P2P,
        metrics_port=free_port,
        port=next(port_allocator),
        fake_capture_device=True,
    ) as m:
        response = m._http_client.post(f"http://localhost:{m.metrics_port}/metrics")
        assert response.status_code == 400  # Bad Request


def test_get_metrics_method(free_port, port_allocator):
    """Confirm that the get_metrics method works correctly."""
    with Momo(
        mode=MomoMode.P2P,
        metrics_port=free_port,
        port=next(port_allocator),
    ) as m:
        # Use the get_metrics method.
        metrics = m.get_metrics()

        assert isinstance(metrics, dict)
        assert "version" in metrics
        assert "libwebrtc" in metrics
