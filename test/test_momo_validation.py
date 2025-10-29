"""Test the mode-specific options validation of Momo."""

import pytest

from momo import Momo, MomoMode


def test_p2p_mode_with_sora_options_raises_error():
    """Confirm that specifying sora mode options in p2p mode raises an error."""
    with pytest.raises(ValueError) as exc_info:
        with Momo(
            mode=MomoMode.P2P,
            # This is an option for sora mode only.
            signaling_urls="wss://example.com/signaling",
            channel_id="test-channel",
        ):
            pass

    assert "Invalid options specified for P2P mode" in str(exc_info.value)
    assert "signaling_urls" in str(exc_info.value)
    assert "channel_id" in str(exc_info.value)
    assert "sora mode" in str(exc_info.value)


def test_p2p_mode_with_ayame_options_raises_error():
    """Confirm that specifying ayame mode options in p2p mode raises an error."""
    with pytest.raises(ValueError) as exc_info:
        with Momo(
            mode=MomoMode.P2P,
            # This is an option for ayame mode only.
            room_id="test-room",
            client_id="test-client",
        ):
            pass

    assert "Invalid options specified for P2P mode" in str(exc_info.value)
    assert "room_id" in str(exc_info.value)
    assert "client_id" in str(exc_info.value)
    assert "ayame" in str(exc_info.value)  # "ayame/sora mode" is also possible.


def test_sora_mode_with_p2p_options_raises_error():
    """Confirm that specifying p2p mode options in sora mode raises an error."""
    with pytest.raises(ValueError) as exc_info:
        with Momo(
            mode=MomoMode.SORA,
            signaling_urls="wss://example.com/signaling",  # Required for sora.
            channel_id="test-channel",  # Required for sora.
            # This is an option for p2p mode only.
            document_root="/var/www/html",
        ):
            pass

    assert "Invalid options specified for Sora mode" in str(exc_info.value)
    assert "document_root" in str(exc_info.value)
    assert "p2p mode" in str(exc_info.value)


def test_sora_mode_with_ayame_options_raises_error():
    """Confirm that specifying ayame mode options in sora mode raises an error."""
    with pytest.raises(ValueError) as exc_info:
        with Momo(
            mode=MomoMode.SORA,
            signaling_urls="wss://example.com/signaling",  # Required for sora.
            channel_id="test-channel",  # Required for sora.
            # This is an option for ayame mode only.
            room_id="test-room",
        ):
            pass

    assert "Invalid options specified for Sora mode" in str(exc_info.value)
    assert "room_id" in str(exc_info.value)
    assert "ayame mode" in str(exc_info.value)  # "ayame/sora mode" is also possible.


def test_ayame_mode_with_p2p_options_raises_error():
    """Confirm that specifying p2p mode options in ayame mode raises an error."""
    with pytest.raises(ValueError) as exc_info:
        with Momo(
            mode=MomoMode.AYAME,
            ayame_signaling_url="wss://example.com/signaling",  # Required for ayame.
            room_id="test-room",  # Required for ayame.
            # This is an option for p2p mode only.
            document_root="/var/www/html",
        ):
            pass

    assert "Invalid options specified for Ayame mode" in str(exc_info.value)
    assert "document_root" in str(exc_info.value)
    assert "p2p" in str(exc_info.value)  # "p2p/sora mode" is also possible.


def test_ayame_mode_with_sora_options_raises_error():
    """Confirm that specifying sora mode options in ayame mode raises an error."""
    with pytest.raises(ValueError) as exc_info:
        with Momo(
            mode=MomoMode.AYAME,
            ayame_signaling_url="wss://example.com/signaling",  # Required for ayame.
            room_id="test-room",  # Required for ayame.
            # This is an option for sora mode only.
            role="sendonly",
            simulcast=True,
        ):
            pass

    assert "Invalid options specified for Ayame mode" in str(exc_info.value)
    assert "role" in str(exc_info.value)
    assert "simulcast" in str(exc_info.value)
    assert "sora mode" in str(exc_info.value)


def test_common_options_allowed_in_all_modes(free_port, port_allocator):
    """Confirm that common options are allowed in all modes."""
    # Use common options in p2p mode.
    with Momo(
        mode=MomoMode.P2P,
        metrics_port=free_port,
        port=next(port_allocator),
        fake_capture_device=True,
        resolution="QVGA",  # Common options.
        framerate=15,  # Common options.
        log_level="info",  # Common options.
    ) as m:
        data = m.get_metrics()
        assert "version" in data


def test_p2p_mode_with_ayame_direction_raises_error():
    """Confirm that specifying Ayame's direction option in p2p mode raises an error."""
    with pytest.raises(ValueError) as exc_info:
        with Momo(
            mode=MomoMode.P2P,
            # This is an option for ayame mode only.
            direction="sendonly",
        ):
            pass

    assert "Invalid options specified for P2P mode" in str(exc_info.value)
    assert "direction" in str(exc_info.value)
    assert "ayame mode" in str(exc_info.value)


def test_sora_mode_with_ayame_direction_raises_error():
    """Confirm that specifying Ayame's direction option in sora mode raises an error."""
    with pytest.raises(ValueError) as exc_info:
        with Momo(
            mode=MomoMode.SORA,
            signaling_urls="wss://example.com/signaling",
            channel_id="test-channel",
            # This is an option for ayame mode only.
            direction="recvonly",
        ):
            pass

    assert "Invalid options specified for Sora mode" in str(exc_info.value)
    assert "direction" in str(exc_info.value)
    assert "ayame mode" in str(exc_info.value)
