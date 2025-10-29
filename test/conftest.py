import itertools
import os
import time
import uuid
from dataclasses import dataclass

import jwt
import pytest
from dotenv import load_dotenv

# Read the .env file.
load_dotenv()


@dataclass
class SoraSettings:
    """Sora mode settings."""

    signaling_urls: str
    channel_id_prefix: str
    secret_key: str
    channel_id: str
    metadata: dict


@pytest.fixture
def sora_settings():
    """A fixture that provides Sora mode settings (generates a new channel_id for each test)."""
    # Get settings from environment variables (required).
    signaling_urls = os.environ.get("TEST_SORA_MODE_SIGNALING_URLS")
    if not signaling_urls:
        raise ValueError("TEST_SORA_MODE_SIGNALING_URLS environment variable is required")

    # Convert comma-separated to space-separated.
    if "," in signaling_urls:
        signaling_urls = signaling_urls.replace(",", " ")

    channel_id_prefix = os.environ.get("TEST_SORA_MODE_CHANNEL_ID_PREFIX")
    if not channel_id_prefix:
        raise ValueError("TEST_SORA_MODE_CHANNEL_ID_PREFIX environment variable is required")

    secret_key = os.environ.get("TEST_SORA_MODE_SECRET_KEY")
    if not secret_key:
        raise ValueError("TEST_SORA_MODE_SECRET_KEY environment variable is required")

    # Generate a channel ID.
    channel_id = f"{channel_id_prefix}{uuid.uuid4().hex[:8]}"

    # Generate metadata.
    payload = {
        "channel_id": channel_id,
        "exp": int(time.time()) + 300,
    }
    access_token = jwt.encode(payload, secret_key, algorithm="HS256")
    metadata = {"access_token": access_token}

    return SoraSettings(
        signaling_urls=signaling_urls,
        channel_id_prefix=channel_id_prefix,
        secret_key=secret_key,
        channel_id=channel_id,
        metadata=metadata,
    )


@pytest.fixture(scope="session")
def port_allocator():
    """A port number allocator shared by all sessions.

    Generates port numbers starting from 55000, which is the starting port number for ephemeral ports.
    Even if multiple tests are executed in parallel, a unique port number is assigned to each test.
    """
    return itertools.count(55000)


@pytest.fixture
def free_port(port_allocator):
    """A fixture that provides a free port number.

    When used in a test function, a unique port number is automatically assigned.
    """
    return next(port_allocator)
