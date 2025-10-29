# Momo E2E Test

This is a pytest-based E2E test using Momo's statistics HTTP API.

## Setup

```bash
# Install dependencies
uv sync
```

## Formatter

```bash
uv run ruff format
```

## Build

Before running tests, you must build momo:

```bash
# For macOS ARM64
python3 run.py build macos_arm64

# For other platforms
python3 run.py build <target>
```

## Run tests

```bash
# Run all tests
uv run pytest

# Run with verbose output
uv run pytest -v

# Run a specific test
uv run pytest test_momo.py::test_metrics_endpoint_returns_200
```

## Build target autodetection

When running tests, prebuilt targets in the `_build` directory are automatically detected.

### Detection Logic

1. **Single Build**
If there is only one build in the `_build` directory, that build will be automatically selected.

2. **For multiple builds**
The platform (OS) and architecture (CPU) of the execution environment are determined, and the appropriate build is selected in the following order of priority:

**macOS (Darwin)**
- ARM64 (Apple Silicon) environment:
1. `macos_arm64`
2. `macos_x86_64` (for execution with Rosetta 2)
- x86_64 (Intel) environment:
1. `macos_x86_64`
2. `macos_arm64`

**Linux**
- ARM64 (aarch64) environment:
1. `ubuntu-24.04_armv8`
2. `ubuntu-22.04_armv8`
3. `ubuntu-20.04_armv8`
- x86_64 environment:
1. `ubuntu-24.04_x86_64`
2. `ubuntu-22.04_x86_64`
3. `ubuntu-20.04_x86_64`

3. **Fallback**
If no matching build is found in the priority list, the first available build is used.

## Test Contents

### P2P Mode

P2P mode runs without any additional configuration. Tests include:

- HTTP metrics endpoint (`/metrics`) test
- Status code check
- JSON response structure check
- W3C WebRTC statistics format check
- Error handling check

### Ayame Mode

Ayame mode tests are run using [Ayame Labo](https://ayame-labo.shiguredo.app/). No additional configuration is required, and it runs as is.

#### Test execution

```bash
# Run Ayame mode tests
uv run pytest test_ayame_mode.py -v

# Run specific tests
uv run pytest test_ayame_mode.py::test_ayame_mode_basic -v
```

#### Ayame mode test contents

- **Basic operation test**
- Verify Ayame mode startup
- Verify Metrics API operation
- Generate and connect to a random room ID

- **Test specifying a client ID**
- Verify connection with a custom client ID

- **Test customizing video settings**
- Set resolution (QVGA, VGA, etc.)
- Set frame rate (15fps, 30fps, 60fps, etc.)
- Set encoder (VP8, etc.)

- **Test customizing audio settings**
- Disable echo cancellation
- Disable auto gain control
- Disable noise suppression

- **P2P connection test**
- Establish bidirectional communication between two peers
- Check video stream transmission and reception
- Check packet transmission and reception statistics

#### Ayame mode operation

- Send a virtual audio/video stream using the `--fake-capture-device` option
- Automatically generate a unique room ID for each test (UUID)
- Dynamically assign a port to the metrics server
- Verify connection status using WebRTC statistics

### Sora mode

To run Sora mode tests, you need to configure the `.env` file.

#### Setup

1. Copy `.env.template` to `.env`

```bash
cp .env.template .env
```

2. Edit the `.env` file and set the actual Sora server information.

```bash
TEST_SORA_MODE_SIGNALING_URLS=wss://your-sora-server.com/signaling
TEST_SORA_MODE_CHANNEL_ID_PREFIX=test_
TEST_SORA_MODE_SECRET_KEY=your_secret_key
```

> [!NOTE]
> - The channel ID is automatically generated (prefix + UUID).
> - Multiple Signaling URLs can be specified, separated by commas.

3. Test Sora mode

```bash
uv run pytest test_sora.py -v
```

#### How Sora mode works

- Send virtual audio and video streams using the `--fake-capture-device` option.
- Start in send-only mode with `--role sendonly`.
- Enable media streams with `--audio true --video true`.
- The metrics server runs on port 8081.

> [!IMPORTANT]
> Sora mode tests are automatically skipped if the environment variable `TEST_SORA_MODE_SIGNALING_URLS` is not set.

## File Structure

```plaintext
test/
├── .env.template # Environment variable template
├── .gitignore # Git exclusion settings
├── .python-version # Python version specification
├── conftest.py # Pytest settings (HTTP client)
├── momo.py # Momo process management class
├── pyproject.toml # Project settings and dependencies
├── test_ayame_mode.py # Ayame mode test
├── test_metrics_api.py # Metrics API test
├── test_p2p_mode.py # P2P mode test
├── test_momo_validation.py # Mode-specific option validation test
├── test_sora_mode.py # Sora mode test
├── uv.lock # UV lock file
└── README.md # This file
```

## Automated testing with GitHub Actions

E2E tests are automatically run with GitHub Actions (`.github/workflows/e2e-test.yml`).

### Execution Triggers

1. **Automatic Execution on Push**
- When `test/**/*.py` or `.github/workflows/e2e-test.yml` is changed
- Automatically retrieves and tests the latest successful build of the pushed branch

2. **Manual Execution (workflow_dispatch)**
- Can be manually triggered from the GitHub Actions UI
- Runs tests by specifying the build artifact URL
- Example: `https://github.com/shiguredo/momo/actions/runs/{run_id}/artifacts/{artifact_id}`

3. **Call from Another Workflow (workflow_call)**
- Automatically called from a build workflow

### Test Environment

- **Target Platforms**:
- Ubuntu 22.04 x86_64
- Ubuntu 24.04 x86_64

- **Environment Variables (GitHub Secrets)**:
- `TEST_SIGNALING_URLS`: Sora signaling URL
- `TEST_CHANNEL_ID_PREFIX`: Channel ID prefix
- `TEST_SECRET_KEY`: Secret key for authentication

### Automatic Artifact Retrieval

- When pushing: Automatically detect the latest successful build of that branch
- When manually running: Download artifacts from the specified URL
- When workflow_call: Use the artifact generated in the previous step

## Troubleshooting

### If momo doesn't start

- Verify that the build is complete
- Verify that the executable exists in `_build/<target>/release/momo/momo`
- Verify that ports 8080 and 8081 are not in use by other processes

### If the Sora mode test fails

- Verify that the Signaling URL is correct
- Verify that the network connection is correct
- Verify that the Sora server is running