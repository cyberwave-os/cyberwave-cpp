# C++ SDK Tests

This directory contains tests for the Cyberwave C++ SDK.

## Test Structure

The tests are designed to verify:

1. **SDK Generation** - The REST SDK can be generated from OpenAPI spec
2. **Compilation** - The SDK compiles successfully with CMake
3. **API Connectivity** - The SDK can connect to the Cyberwave backend
4. **Basic Operations** - Core functionality like listing workspaces works
5. **Error Handling** - Proper error handling and exception management
6. **MQTT client** - `test_mqtt`: connect, subscribe to a topic, disconnect (skips if no broker; use `CYBERWAVE_MQTT_HOST`, `CYBERWAVE_MQTT_PORT`, `CYBERWAVE_API_KEY`)

## Running Tests with Docker (Recommended)

The easiest way to run tests is using Docker, which provides a consistent environment without needing to install C++ dependencies on your host machine.

### Quick Start

```bash
# 1. Start the backend services
cd cyberwave-backend
docker compose -f local.yml up -d django postgres redis mosquitto

# Wait for services to be ready (about 30 seconds)
# Verify backend is ready:
curl http://localhost:8000/api/v1/openapi.json

# 2. Generate the C++ SDK (uses Docker internally)
cd ../cyberwave-sdks
./cpp-sdk-gen.sh --host host.docker.internal
or
./cpp-sdk-gen.sh --host localhost

Make sure the test token is defined as an env variable.
E.g. CYBERWAVE_API_KEY

# 3. Build and run tests in Docker
cd cyberwave-cpp
docker build -f tests/Dockerfile -t cyberwave-cpp-tests .
docker run --rm \
  -e CYBERWAVE_BASE_URL=http://host.docker.internal:8000 \
  -e CYBERWAVE_API_KEY=$CYBERWAVE_API_KEY \
  cyberwave-cpp-tests

# 4. Cleanup
cd ../../../cyberwave-backend
docker compose -f local.yml down -v
```

### Platform-Specific Notes

**macOS/Windows (Docker Desktop):**

- Use `host.docker.internal` - Docker Desktop automatically handles this
- This is the default and recommended approach

**Linux:**

- Add `--add-host=host.docker.internal:172.17.0.1` to the docker run command:
  ```bash
  docker run --rm \
    --add-host=host.docker.internal:172.17.0.1 \
    -e CYBERWAVE_BASE_URL=http://host.docker.internal:8000 \
    -e CYBERWAVE_API_KEY=$CYBERWAVE_API_KEY \
    cyberwave-cpp-tests
  ```
- Or use `--network host` and change URL to `http://localhost:8000`

### Development Workflow

When making changes to tests:

```bash
# Quick way: Use the helper script
cd cyberwave-cpp-sdk/tests
./run-tests.sh

# With rebuild
./run-tests.sh --rebuild

# Manual way
cd cyberwave-cpp-sdk
docker build -f tests/Dockerfile -t cyberwave-cpp-tests .
docker run --rm \
  -e CYBERWAVE_BASE_URL=http://host.docker.internal:8000 \
  -e CYBERWAVE_API_KEY=$CYBERWAVE_API_KEY \
  cyberwave-cpp-tests
```

## Running Tests Locally (Without Docker)

If you prefer to run tests directly on your host machine:

### Prerequisites

**Ubuntu/Debian:**

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libcpprest-dev \
    libssl-dev \
    libboost-all-dev \
    nlohmann-json3-dev
```

**macOS:**

```bash
brew install cmake cpprestsdk openssl boost nlohmann-json
```

### Build and Run

```bash
# 1. Start backend
cd cyberwave-backend
docker compose -f local.yml up -d

# 2. Generate SDK
cd ../cyberwave-sdks
./cpp-sdk-gen.sh

# 3. Build REST SDK library
cd cyberwave-cpp-sdk/rest
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install

# 4. Build and run tests
cd ../../tests
mkdir -p build && cd build
cmake ..
make -j$(nproc)
export CYBERWAVE_BASE_URL=http://localhost:8000
export CYBERWAVE_API_KEY=$CYBERWAVE_API_KEY
./test_sdk
```

## Test Coverage

Current tests cover:

- ✅ SDK structure and headers
- ✅ API configuration creation
- ✅ API client instantiation
- ✅ Backend connectivity
- ✅ Basic operations (list workspaces)
- ✅ Error handling (invalid authentication)

Future tests should cover:

- ⏳ CRUD operations (create, read, update, delete)
- ⏳ Resource management (projects, assets, twins)
- ⏳ Twin control operations
- ⏳ Joint state updates
- ⏳ File uploads
- ⏳ MQTT client (when implemented)
- ⏳ Async operations
- ⏳ Rate limiting and retries

## Writing New Tests

Tests are defined in `test_sdk.cpp`. To add a new test:

```cpp
/**
 * Test N: Description of what this test does
 */
bool test_my_feature() {
    printTestHeader(N, "My Feature Test");

    try {
        // Setup
        std::string baseUrl = getEnvVar("CYBERWAVE_BASE_URL", "http://localhost:8000");
        std::string apiKey = getEnvVar("CYBERWAVE_API_KEY", "test-api-key");

        auto config = std::make_shared<ApiConfiguration>();
        config->setBaseUrl(utility::conversions::to_string_t(baseUrl + "/api/v1"));
        config->setApiKey(U("Authorization"), utility::conversions::to_string_t("Api-Key " + apiKey));

        auto apiClient = std::make_shared<ApiClient>(config);
        auto api = std::make_shared<DefaultApi>(apiClient);

        // Test logic
        printInfo("Testing something...");
        // ... your test code ...

        printSuccess("Test passed");
        return true;
    } catch (const std::exception& e) {
        printFailure(std::string("Test failed: ") + e.what());
        return false;
    }
}
```

Then add it to the `main()` function:

```cpp
allPassed &= test_my_feature();
```

## CI/CD Integration

The tests are automatically run by GitHub Actions on:

- Pull requests that modify the C++ SDK
- Pushes to `dev` and `production` branches

See `.github/workflows/sdk-cpp-test-and-release.yml` for the full CI/CD pipeline.

## Troubleshooting

### Backend Not Ready

```bash
# Check if backend is responding
curl http://localhost:8000/api/v1/openapi.json

# View backend logs
cd cyberwave-backend
docker compose -f local.yml logs django
```

### Connection Errors

```bash
# Test connectivity from within a container (macOS/Windows)
docker run --rm curlimages/curl:latest \
  curl http://host.docker.internal:8000/api/v1/openapi.json

# Test connectivity from within a container (Linux)
docker run --rm --add-host=host.docker.internal:172.17.0.1 \
  curlimages/curl:latest \
  curl http://host.docker.internal:8000/api/v1/openapi.json
```

### Docker Build Errors

```bash
# Clean rebuild
cd cyberwave-cpp-sdk
docker build --no-cache -f tests/Dockerfile -t cyberwave-cpp-tests .
```

### Local Build Errors

```bash
# Make sure REST SDK is installed
cd cyberwave-cpp-sdk/rest/build
sudo make install
sudo ldconfig

# Check library is installed
ldconfig -p | grep CppRestOpenAPIClient
```

### CMake Can't Find Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install pkg-config

# macOS
brew install pkg-config

# Check pkg-config can find cpprestsdk
pkg-config --cflags --libs cpprestsdk
```

## Files

- `test_sdk.cpp` - Main test suite
- `CMakeLists.txt` - CMake configuration for building tests
- `Dockerfile` - Multi-stage Docker build for tests
- `.dockerignore` - Files to exclude from Docker build
- `run-tests.sh` - Helper script to run tests in Docker
- `README.md` - This file
