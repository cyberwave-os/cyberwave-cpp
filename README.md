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
├── mqtt/                        # Paho MQTT adapter
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
  libcpprest-dev libssl-dev libboost-all-dev \
  nlohmann-json3-dev libpaho-mqtt-dev libpaho-mqttpp-dev libspdlog-dev
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
| `paho-mqtt-cpp` + `nlohmann_json` + `spdlog` | For MQTT | Real-time pub/sub |
| `OpenCV` | Optional | `camera_stream_opencv` example (`./install.sh --with-opencv`) |
| `libdatachannel` | Optional | WebRTC camera streaming |
| FFmpeg (`libavcodec`, `libavutil`, `libswscale`) | Optional | H264 encoding in CameraStreamer |

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

### MQTT

```cpp
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/paho_mqtt_adapter.h>

int main() {
    cyberwave::Config config;
    config.load_from_environment();

    cyberwave::Client client(config);
    auto mqtt = std::make_shared<cyberwave::PahoMqttAdapter>(config);
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
| `cyberwave::Twin` | High-level twin handle |
| `cyberwave::TwinManager` | List / get / create / update / delete twins |
| `cyberwave::JointController` | Joint get / set / list / get_all |
| `cyberwave::WorkflowManager` | List / get / trigger workflows |
| `cyberwave::PahoMqttAdapter` | Paho-backed `IMqttClient` implementation |

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
