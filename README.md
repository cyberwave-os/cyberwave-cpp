<p align="center">
  <a href="https://cyberwave.com">
    <img src="https://cyberwave.com/cyberwave-logo-black.svg" alt="Cyberwave logo" width="240" />
  </a>
</p>

# Cyberwave C++ SDK (experimental)

Official C++ SDK for the [Cyberwave](https://cyberwave.com) platform. Control digital twins, manage simulations, and interact with the Cyberwave ecosystem from C++ applications.

[![License](https://img.shields.io/badge/License-MIT-orange.svg)](https://github.com/cyberwave-os/cyberwave-cpp/blob/main/LICENSE)
[![Documentation](https://img.shields.io/badge/Documentation-docs.cyberwave.com-orange)](https://docs.cyberwave.com)
[![Discord](https://badgen.net/badge/icon/discord?icon=discord&label&color=orange)](https://discord.gg/dfGhNrawyF)

## Overview

The SDK provides three integration layers:

1. **REST API** — Auto-generated client from the Cyberwave OpenAPI spec (in `rest/`), wrapped with a hand-crafted C++ layer for a clean developer experience.
2. **MQTT API** — Real-time pub/sub communication with the Cyberwave backend (in `mqtt/`).
3. **Edge abstractions** — Base classes for building edge nodes and AMR integrations (in `include/cyberwave/edge/`).
4. **Data plane (first milestone)** — Wire-format helpers, validated data keys, and a filesystem-backed `DataBus` facade for JSON/bytes/ndarray payload exchange (in `include/cyberwave/data.h`).
5. **Worker/manifest scaffolding** — Hook registration helpers plus a manifest schema/dispatch helper for upcoming worker-runtime parity (in `include/cyberwave/workers.h` and `include/cyberwave/manifest.h`).

## Project Structure

```
cyberwave-cpp/
├── rest/                        # Auto-generated REST client (included in releases)
├── include/cyberwave/           # Public SDK headers
│   ├── client.h                 # Main client (REST + optional MQTT)
│   ├── twin.h / twins.h        # Twin abstractions
│   ├── config.h                 # Configuration / env loading
│   ├── exceptions.h             # Error types
│   ├── edge/                    # Edge node base classes
│   └── ...                      # Assets, environments, workflows, alerts, etc.
├── src/cyberwave/               # Implementation files
├── mqtt/                        # MQTT client (libmosquitto)
├── cmake/                       # CMake package config template
├── examples/                    # Example applications
├── tests/                       # Unit and integration tests
├── install.sh                   # One-command installer (deps + build + install)
└── Dockerfile                   # Clean-room install validation
```

## Installation

### One-command install (recommended)

```bash
./install.sh
```

This handles everything: system dependencies (Ubuntu/Debian via apt), REST client generation if `rest/` is missing, CMake build, and system-wide install to `/usr/local`.

Common options:

```bash
./install.sh --prefix /opt/cyberwave        # Custom install prefix
./install.sh --skip-deps                    # Skip apt if deps are already installed
./install.sh --force-generate-rest          # Regenerate rest/ from OpenAPI
./install.sh --openapi-url http://localhost:8000/api/v1/openapi.json  # Custom endpoint
./install.sh --with-opencv                  # Also install OpenCV (for camera_stream_opencv example)
./install.sh --run-tests                    # Run ctest after build
```

The script is idempotent and safe to re-run. Run `./install.sh --help` for all options.

### Manual install

Install dependencies:

```bash
# Ubuntu/Debian
sudo apt-get update && sudo apt-get install -y \
  build-essential cmake pkg-config ca-certificates curl git \
  libcpprest-dev libssl-dev libboost-dev \
  libboost-system-dev libboost-thread-dev libboost-chrono-dev \
  libboost-filesystem-dev libboost-random-dev \
  nlohmann-json3-dev libmosquitto-dev libspdlog-dev
```

Build and install:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

> **Note:** The `rest/` directory ships pre-generated in releases. If you are building from source and `rest/` is missing, run `./install.sh --force-generate-rest` or the monorepo's `cpp-sdk-gen.sh` to generate it.

### Dependency Overview

| Dependency | Required | Purpose |
|---|---|---|
| `cpprestsdk` | Yes | HTTP client for REST API |
| `OpenSSL` | Yes | TLS |
| `libmosquitto` + `nlohmann_json` + `spdlog` | For MQTT | Real-time pub/sub |
| `OpenCV` | Optional | `camera_stream_opencv` example (`./install.sh --with-opencv`) |
| `libdatachannel` | Optional | WebRTC in CameraStreamer and EncodedH264CameraStreamer |
| FFmpeg (`libavcodec`, `libavutil`, `libswscale`) | Optional | H264 encoding in CameraStreamer (not required for EncodedH264CameraStreamer passthrough) |

## Using the SDK in Your Project

### find_package (installed SDK)

```cmake
find_package(CyberwaveCppSDK CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE CyberwaveCppSDK::cyberwave_sdk)
```

### add_subdirectory (vendored / in-tree)

```cmake
add_subdirectory(path/to/cyberwave-cpp cyberwave-cpp-build)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cyberwave_sdk)
```

## Quick Start

### Configuration

```cpp
#include <cyberwave/client.h>
#include <cyberwave/config.h>

int main() {
    cyberwave::Config config;
    config.load_from_environment();
    // Or: config.base_url = "https://api.cyberwave.com";
    //     config.api_key  = "your_api_key";

    cyberwave::Client client(config);
    return 0;
}
```

### Working with Twins

```cpp
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/joints.h>
#include <cyberwave/twins.h>

int main() {
    cyberwave::Config config;
    config.load_from_environment();
    cyberwave::Client client(config);

    auto twins = client.twins().list();
    if (twins.empty()) return 0;

    auto robot = twins[0];
    robot.edit_position(1.0, 0.0, 0.5);
    robot.edit_rotation(90.0);

    auto joints = robot.joints().get_all();
    if (!joints.empty()) {
        const auto& name = joints.begin()->first;
        robot.joints().set(name, 30.0, true);
    }

    client.affect("simulation");
    return 0;
}
```

`Client::affect()` now resets any attached MQTT client so reconnects pick up the new runtime settings. For parity with Python, simulation mode defaults frame fetches to `source_type=sim` while control publishers use `sim_tele`; live mode keeps REST/frame defaults on `edge` and control publishers on `tele`.

### Quickstart Twin Resolution

`Client::twin()` can now fetch an existing twin by UUID, or resolve an asset registry id / alias and reuse or create a twin in the configured environment. When no environment is configured, it mirrors Python quickstart behavior by creating and caching a default workspace/project/environment context.

```cpp
cyberwave::TwinResolveOptions options;
options.environment_id = "env-uuid";
options.name = "Front Camera";

auto camera = client.twin("camera", options);
```

Regular `Twin` handles now expose the capability convenience helpers directly, so a twin fetched through `client.twin(...)` or `client.twins().get(...)` can call helpers such as `move_forward()`, `grip()`, or `start_streaming()` without going through a separate subclass factory first.

Scene-level composed schema and environment export helpers now use the canonical backend routes (`/universal-schema.json`, `/urdf-scene.zip`, `/mujoco-scene.zip`) instead of the older `/exports/...` paths.

### Locomotion Policies

Use `client.assets().get_controller_setup_view(asset_uuid)` to read the backend-resolved controller setup for an asset instead of inspecting raw metadata. The typed setup view exposes the asset UUID, primary preview controller key, runtime policy count, `primary_policy_ref()`, `default_policy_ref(runtime_kind, backend)`, and `runtime_target(runtime_kind, backend)`; `get_controller_setup()` remains available when callers need the raw JSON payload. For velocity policy payloads, include `cyberwave/locomotion_contracts.h` and build `locomotion.velocity_command.v1` commands with `build_locomotion_velocity_command()` or `stop_locomotion_velocity_command()`; the canonical schema is tracked at `contracts/locomotion.velocity_command.v1.schema.json`, and `LOCOMOTION_VELOCITY_COMMAND_REQUIRED_FIELDS` exposes the schema-required keys for custom emitters. `Twin::dispatch_velocity()` and convenience helpers such as `Twin::set_velocity()`, `Twin::drive_forward()`, and `Twin::stop_velocity()` send typed commands through the backend Control Agent dispatch endpoint so SDK callers use the same runtime policy resolver and cancellation semantics as the frontend and Python SDK. When a caller must override the backend default, pass a typed `PolicyRefPayload` to `Twin::dispatch_velocity(command, policy_ref, mode, simulation_backend)`.

### Asset uploads

`AssetManager::upload_glb()` now mirrors Python's behavior: small GLBs use the standard multipart endpoint, large GLBs switch to the attachment signed-upload flow automatically, and payload-too-large responses on the direct endpoint retry through the attachment path.

### MQTT

```cpp
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/cyberwave_mqtt_adapter.h>

int main() {
    cyberwave::Config config;
    config.load_from_environment();

    cyberwave::Client client(config);
    auto mqtt = std::make_shared<cyberwave::CyberwaveMqttAdapter>(config);
    mqtt->connect();
    client.set_mqtt_client(mqtt);

    auto twins = client.twins().list();
    if (!twins.empty()) {
        twins[0].subscribe_position([](const std::string& payload) {
            std::cout << "Position: " << payload << std::endl;
        });
    }

    client.disconnect();
    mqtt->disconnect();
    return 0;
}
```

For local non-TLS brokers: `export CYBERWAVE_MQTT_USE_TLS=false`

Optional MQTT envs:
- `CYBERWAVE_MQTT_PROTOCOL=5` to request MQTT v5
- `CYBERWAVE_TWIN_UUID=<uuid>` to keep a default twin UUID in config

#### GPS Telemetry

Publish raw GNSS data for twins equipped with a GPS receiver. GPS data is
stored in the backend as `twin_gps_update` telemetry events.

```cpp
cyberwave::GpsFix fix;
fix.latitude  = 37.7749;
fix.longitude = -122.4194;
fix.altitude  = 10.5;
fix.satellite_count = 12;
fix.signal_level    = 5;
fix.compass_heading = 270.0;

// Via CyberwaveMQTTClient
mqtt_client.update_twin_gps("twin-uuid", fix);

// Via BaseEdgeNode helper
node.publish_gps("twin-uuid", fix);
```

### DataBus (filesystem backend)

```cpp
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/data.h>

int main() {
    cyberwave::Config config;
    config.twin_uuid = "550e8400-e29b-41d4-a716-446655440000";

    cyberwave::Client client(config);
    auto backend = std::make_shared<cyberwave::FilesystemDataBackend>();
    auto data = client.data(backend);

    data.publish("joint_states", {{"joint1", 0.5}, {"joint2", -1.0}});
    auto latest = data.latest("joint_states");
    return latest.has_value() ? 0 : 1;
}
```

### Error Handling

```cpp
try {
    cyberwave::Client client(config);
    auto robot = client.twin("some-uuid");
    robot.refresh();
} catch (const cyberwave::CyberwaveAPIError& e) {
    std::cerr << "API error: " << e.what() << std::endl;
} catch (const cyberwave::CyberwaveConnectionError& e) {
    std::cerr << "Connection error: " << e.what() << std::endl;
} catch (const cyberwave::CyberwaveError& e) {
    std::cerr << "SDK error: " << e.what() << std::endl;
}
```

## API Reference

| Class | Purpose |
|---|---|
| `cyberwave::Client` | Main SDK client (REST + optional MQTT) |
| `cyberwave::Config` | Configuration and environment loading |
| `cyberwave::TwinResolveOptions` | Quickstart options for resolving or creating twins from asset keys |
| `cyberwave::Twin` | High-level twin handle |
| `cyberwave::TwinManager` | List / get / create / update / delete twins |
| `cyberwave::WorkflowRun` | Workflow run view with polling and MQTT status subscription helpers |
| `cyberwave::JointController` | Joint get / set / list / get_all |
| `cyberwave::WorkflowManager` | List / get / trigger workflows |
| `cyberwave::CyberwaveMqttAdapter` | libmosquitto-backed `IMqttClient` implementation |
| `cyberwave::DataBus` | Typed data-plane facade for publish / subscribe / latest |
| `cyberwave::FilesystemDataBackend` | Stdlib-backed data transport for development and tests |
| `cyberwave::HookRegistry` | Worker hook registration surface for upcoming runtime support |
| `cyberwave::ManifestSchema` | Read-only manifest schema and dispatch-mode validation helpers |

## Examples

See `examples/` for working code aligned with the Python SDK:

| Example | Transport | Description |
|---|---|---|
| `quickstart.cpp` | REST (+MQTT if available) | Minimal end-to-end demo |
| `compact.cpp` | REST | Compact API usage |
| `alerts.cpp` | REST | Alert management |
| `workflows.cpp` | REST | Workflow triggering |
| `capture_frame.cpp` | REST | Single frame capture |
| `camera_stream.cpp` | MQTT | Live camera streaming |
| `camera_stream_opencv.cpp` | MQTT | OpenCV `IFrameSource` reference |
| `depth_stream.cpp` | MQTT | Depth camera streaming |
| `command_receiver_simple.cpp` | MQTT | Receive commands from the platform |
| `go2_locomotion.cpp` | MQTT | Unitree Go2 locomotion |

## Development

### Running Tests

```bash
cmake -S . -B build -DCYBERWAVE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Docker Validation

The root `Dockerfile` is a clean-room validation: it runs `./install.sh` in a fresh Ubuntu image, then builds a downstream project using `find_package(CyberwaveCppSDK CONFIG REQUIRED)`.

```bash
docker build -t cyberwave-cpp-install-validation .
```

Build succeeds only if installation and downstream package discovery work end-to-end.

### Development Container

`Dockerfile.dev` is the reproducible SDK development image. It installs the optional gRPC/OpenCV/media build dependencies, regenerates `rest/` from OpenAPI, then builds and runs the C++ test suite.

```bash
docker build -f Dockerfile.dev -t cyberwave-cpp-dev \
  --build-arg OPENAPI_URL=http://host.docker.internal:8000/api/v1/openapi.json .
```

You can point `OPENAPI_URL` at a local backend or keep the default production schema.

## Requirements

- C++17 or later
- CMake 3.10+
- Linux (Ubuntu/Debian fully supported; macOS manual deps)

## Support

- **Documentation**: https://docs.cyberwave.com
- **Issues**: https://github.com/cyberwave-os/cyberwave-cpp/issues
- **Community**: https://discord.gg/dfGhNrawyF

## License

MIT License. See [LICENSE](LICENSE) for details.
