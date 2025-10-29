"""End-to-end test for P2P mode."""

from momo import Momo, MomoMode


def test_with_custom_arguments(free_port, port_allocator):
    """Confirm that momo can be started with custom arguments."""
    with Momo(
        mode=MomoMode.P2P,
        metrics_port=free_port,
        port=next(port_allocator),
        fake_capture_device=True,
        resolution="QVGA",
        framerate=15,
        log_level="info",
    ) as m:
        data = m.get_metrics()
        assert "version" in data


def test_multiple_instances_concurrent(port_allocator):
    """Confirm that multiple momo instances can be started concurrently."""
    with Momo(
        mode=MomoMode.P2P,
        metrics_port=next(port_allocator),
        port=next(port_allocator),
        fake_capture_device=True,
    ) as m1:
        with Momo(
            mode=MomoMode.P2P,
            metrics_port=next(port_allocator),
            port=next(port_allocator),
            fake_capture_device=True,
        ) as m2:
            # Confirm that both instances are working correctly.
            data1 = m1.get_metrics()
            data2 = m2.get_metrics()

            assert "version" in data1
            assert "version" in data2


def test_multiple_instances_different_configs(port_allocator):
    """Confirm that multiple instances can be started with different configurations."""
    with Momo(
        mode=MomoMode.P2P,
        metrics_port=next(port_allocator),
        port=next(port_allocator),
        fake_capture_device=True,
        resolution="QVGA",
        framerate=15,
    ) as m1:
        with Momo(
            mode=MomoMode.P2P,
            metrics_port=next(port_allocator),
            port=next(port_allocator),
            fake_capture_device=True,
            resolution="QVGA",
            framerate=15,
        ) as m2:
            # Confirm that both instances are accessible.
            data1 = m1.get_metrics()
            data2 = m2.get_metrics()
            assert "version" in data1
            assert "version" in data2


def test_dynamic_instance_creation_and_cleanup(port_allocator):
    """Confirm that instances can be created and deleted dynamically."""
    instances = []

    try:
        for i in range(3):
            momo = Momo(
                mode=MomoMode.P2P,
                metrics_port=next(port_allocator),
                port=next(port_allocator),
                fake_capture_device=True,
            )
            momo.__enter__()
            instances.append(momo)

            # Confirm that the newly created instance is working correctly.
            data = momo.get_metrics()
            assert "version" in data

        # Confirm that all instances are working correctly at the same time.
        for momo in instances:
            data = momo.get_metrics()
            assert "version" in data

    finally:
        # Clean up.
        for momo in instances:
            momo.__exit__(None, None, None)
