<p align="center">
  <a href="https://cyberwave.com">
    <img src="https://cyberwave.com/cyberwave-logo-black.svg" alt="Cyberwave logo" width="240" />
  </a>
</p>

# Cyberwave C++ SDK

This module is part of **Cyberwave: Making the physical world programmable**.

Official C++ SDK for the Cyberwave platform. Control digital twins, manage simulations, and interact with the Cyberwave ecosystem from your C++ applications.

[![License](https://img.shields.io/badge/License-MIT-orange.svg)](https://github.com/cyberwave-os/cyberwave-cpp-sdk/blob/main/LICENSE)
[![Documentation](https://img.shields.io/badge/Documentation-docs.cyberwave.com-orange)](https://docs.cyberwave.com)
[![Discord](https://badgen.net/badge/icon/discord?icon=discord&label&color=orange)](https://discord.gg/dfGhNrawyF)

## Overview

This SDK integrates two APIs and one bridge:

1. **REST API** - Auto-generated from OpenAPI specification (in `/rest` folder)
2. **MQTT API** - Real-time communication with Cyberwave backend (in `/mqtt` folder)
3. **ROS2 Bridge** - Real-time communication with ROS2 (in `/ros2` folder)

The REST API provides a hand-crafted layer on top of the auto-generated code to deliver a delightful developer experience with modern C++ idioms. The MQTT API and the ROS2 Bridge have been written from scratch.

## Architecture

```
cyberwave-cpp-sdk/
├── rest/                    # Auto-generated REST API client (do not modify)
├── include/
│   └── cyberwave/
│       ├── client.h         # Main Cyberwave client integrating REST and MQTT
│       ├── twin.h           # High-level Twin abstraction for controlling digital twins
│       ├── twins.h          # Twin manager
│       ├── config.h         # Configuration management
│       └── exceptions.h     # Custom exceptions
└── src/
    └── cyberwave/           # Implementation files
```

## Installation

### Prerequisites

Install the required dependencies:

**macOS:**

```bash
brew install cpprestsdk cmake openssl
# Optional for MQTT examples:
brew install paho-mqtt-cpp nlohmann-json spdlog
```

**Linux (Ubuntu/Debian):**

```bash
sudo apt-get update
sudo apt-get install libcpprest-dev cmake libssl-dev
# Required for built-in MQTT support (optional for REST-only builds):
sudo apt-get install libpaho-mqtt-dev libpaho-mqttpp-dev nlohmann-json3-dev libspdlog-dev
```

**Windows:**

```bash
vcpkg install cpprestsdk cpprestsdk:x64-windows boost-uuid boost-uuid:x64-windows
```

### Dependency Notes (Core vs Optional)

`cyberwave_sdk` keeps transport/media dependencies optional where possible:

- **Core SDK (REST + base types):** `cpprestsdk`, `OpenSSL`
- **MQTT usage/examples:** `paho-mqtt-cpp`, `nlohmann_json`, `spdlog`
- **WebRTC camera streaming:** `libdatachannel` (signaling + RTP)
- **H264 encoding in CameraStreamer:** FFmpeg (`libavcodec`, `libavutil`, `libswscale`)

Frame decoding is intentionally **not** done in the SDK. `IFrameSource` now provides raw frames (`VideoFrame`), so JPEG/driver-specific decoding can live in driver adapters (for example, using OpenCV in the driver), while the SDK remains codec/transport focused.

### Building the SDK

```bash
cd cyberwave-cpp-sdk
cmake -S . -B build
cmake --build build --parallel
```

### Using the SDK in Your Project

The top-level build produces a `cyberwave_sdk` target. A simple in-tree consumer setup looks like this:

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyRoboticsApp)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(path/to/cyberwave-cpp-sdk cyberwave-cpp-sdk-build)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE
    cyberwave_sdk
)
```

## Quick Start

### Basic Configuration

```cpp
#include <cyberwave/client.h>
#include <cyberwave/config.h>

int main() {
    cyberwave::Config config;
    config.load_from_environment();

    // Or set fields explicitly:
    // config.base_url = "http://localhost:8000";
    // config.api_key = "your_api_key";

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
    if (twins.empty()) {
        return 0;
    }

    auto robot = twins[0];
    robot.edit_position(1.0, 0.0, 0.5);
    robot.edit_rotation(90.0); // yaw in degrees

    auto joints = robot.joints().get_all();
    if (!joints.empty()) {
        const auto& joint_name = joints.begin()->first;
        robot.joints().set(joint_name, 30.0, true); // degrees=true
        double angle_rad = robot.joints().get(joint_name);
    }

    client.affect("simulation");
    return 0;
}
```

### MQTT Usage

`Client` holds MQTT via `std::shared_ptr<IMqttClient>`. Use `PahoMqttAdapter` as the concrete client.

```cpp
#include <memory>
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
        auto robot = twins[0];
        robot.subscribe_position([](const std::string& payload) {
            std::cout << "Position: " << payload << std::endl;
        });
    }

    client.disconnect(); // detaches MQTT from Client; call set_mqtt_client again to reuse
    mqtt->disconnect();
    return 0;
}
```

For local non-TLS brokers on port `1883`, set:

```bash
export CYBERWAVE_MQTT_USE_TLS=false
```

### Error Handling

```cpp
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>

int main() {
    try {
        cyberwave::Config config;
        config.load_from_environment();
        cyberwave::Client client(config);
        auto robot = client.twin("invalid/twin");
        robot.refresh();
    }
    catch (const cyberwave::CyberwaveAPIError& e) {
        std::cerr << "API error: " << e.what() << std::endl;
    }
    catch (const cyberwave::CyberwaveConnectionError& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
    }
    catch (const cyberwave::CyberwaveError& e) {
        std::cerr << "SDK error: " << e.what() << std::endl;
    }
}
```

## Main Entry Points

- `cyberwave::Client` - main SDK client
- `cyberwave::Config` - SDK configuration / env loading
- `cyberwave::Twin` - high-level twin handle
- `cyberwave::JointController` - joint get/set/list/get_all
- `cyberwave::TwinManager` - list/get/create/update/delete twins
- `cyberwave::WorkflowManager` - list/get/trigger workflows
- `cyberwave::PahoMqttAdapter` - concrete MQTT implementation for `IMqttClient`

## Examples

See the `/examples` directory for working examples aligned with the Python SDK:

- `quickstart.cpp`
- `compact.cpp`
- `alerts.cpp`
- `workflows.cpp`
- `capture_frame.cpp`
- `camera_stream.cpp`
- `camera_stream_opencv.cpp` (OpenCV-based `IFrameSource` implementation)
- `depth_stream.cpp`
- `command_receiver_simple.cpp`
- `go2_locomotion.cpp`

`camera_stream_opencv.cpp` is the reference for implementing a driver-side frame source with OpenCV and feeding raw `VideoFrame` data into `CameraStreamer`.

## Development

### Project Structure

The SDK follows a clear separation between auto-generated and hand-crafted code:

- **`/rest`** - Auto-generated REST API client (gitignored, regenerated on build)
- **`/include/cyberwave`** - Public headers for hand-crafted SDK layer
- **`/src/cyberwave`** - Implementation of hand-crafted SDK layer
- **`/examples`** - Example applications
- **`/tests`** - Unit and integration tests

### Building from Source

```bash
git clone https://github.com/cyberwave-os/cyberwave-cpp-sdk.git
cd cyberwave-cpp-sdk
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Running Tests

```bash
ctest --test-dir build --output-on-failure
```

### Testing with Docker

Build and run tests inside the provided Docker image. Use `--network host` so backend-dependent tests can reach a local backend:

```bash
docker build -t cyberwave-cpp-sdk .
docker run --rm --network host \
  -e CYBERWAVE_BASE_URL=http://127.0.0.1:8000 \
  -e CYBERWAVE_API_KEY=your-api-key \
  cyberwave-cpp-sdk
```

### Running Examples with Docker

REST examples:

```bash
docker run --rm --network host \
  -e CYBERWAVE_API_KEY=your-api-key \
  -e CYBERWAVE_BASE_URL=http://127.0.0.1:8000 \
  cyberwave-cpp-sdk /src/build/examples/example_compact
```

MQTT-enabled examples:

```bash
docker run --rm --network host \
  -e CYBERWAVE_API_KEY=your-api-key \
  -e CYBERWAVE_BASE_URL=http://127.0.0.1:8000 \
  -e CYBERWAVE_MQTT_HOST=localhost \
  -e CYBERWAVE_MQTT_PORT=1883 \
  -e CYBERWAVE_MQTT_USERNAME=test \
  -e CYBERWAVE_MQTT_USE_TLS=false \
  cyberwave-cpp-sdk /src/build/examples/example_quickstart
```

The Docker image includes the MQTT dependencies, so `example_quickstart`, `example_camera_stream`,
`example_depth_stream`, `example_command_receiver_simple`, and `example_go2_locomotion` are built too.

## Requirements

- **C++17** or later
- **CMake 3.10** or later
- **cpprestsdk** (Microsoft C++ REST SDK)
- **OpenSSL**
- **Required for built-in MQTT support:** Paho MQTT C/C++, nlohmann-json, spdlog
  - Optional only for REST-only builds that do not use `PahoMqttAdapter` or MQTT-enabled examples

## Contributing

Contributions are welcome. If you find an issue or want to improve the SDK, open an issue or submit a pull request. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

This SDK is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Support

- **Documentation**: https://docs.cyberwave.com
- **Issues**: https://github.com/cyberwave-os/cyberwave-cpp-sdk/issues
- **Community**: https://discord.gg/dfGhNrawyF

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for version history and updates.
