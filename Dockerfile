FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

ARG OPENAPI_URL=https://api.cyberwave.com/api/v1/openapi.json

WORKDIR /opt/cyberwave-cpp

# Copy only install.sh first so the apt layer is cached across source changes.
# ccache is included here so it survives alongside the apt deps layer.
COPY install.sh /opt/cyberwave-cpp/install.sh
RUN ./install.sh --deps-only && \
    apt-get install -y --no-install-recommends ccache && \
    rm -rf /var/lib/apt/lists/*

# Now copy the rest of the source tree.
COPY . /opt/cyberwave-cpp

# Build and install the SDK in a clean environment (tests built but not run here).
# --mount=type=cache keeps the ccache directory on the runner's local disk across
# Docker builds; it survives COPY-layer invalidation so incremental CI runs skip
# unchanged translation units even when the source layer is rebuilt from scratch.
# install.sh auto-detects ccache when CMAKE_CXX_COMPILER_LAUNCHER is unset and
# sccache is absent, so no extra flags are needed here.
RUN --mount=type=cache,target=/root/.ccache,sharing=locked \
    ./install.sh --skip-deps --openapi-url "${OPENAPI_URL}" --run-tests

# Build examples separately to verify they compile against the installed SDK.
RUN --mount=type=cache,target=/root/.ccache,sharing=locked \
    cmake -S examples -B /tmp/examples-build \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_PREFIX_PATH=/usr/local && \
    cmake --build /tmp/examples-build --parallel && \
    echo "[Dockerfile] Examples built successfully."

# Validate find_package() from an external downstream project.
RUN mkdir -p /tmp/cyberwave-cpp-consumer && \
    printf '%s\n' \
      'cmake_minimum_required(VERSION 3.16)' \
      'project(cyberwave_cpp_install_validation LANGUAGES CXX)' \
      'set(CMAKE_CXX_STANDARD 17)' \
      'set(CMAKE_CXX_STANDARD_REQUIRED ON)' \
      'find_package(CyberwaveCppSDK CONFIG REQUIRED)' \
      'add_executable(cyberwave_cpp_install_validation main.cpp)' \
      'target_link_libraries(cyberwave_cpp_install_validation PRIVATE CyberwaveCppSDK::cyberwave_sdk)' \
    > /tmp/cyberwave-cpp-consumer/CMakeLists.txt && \
    printf '%s\n' \
      '#include <cyberwave/config.h>' \
      '#include <iostream>' \
      'int main() {' \
      '    cyberwave::Config cfg;' \
      '    std::cout << "cyberwave_cpp_install_validation_ok" << std::endl;' \
      '    return 0;' \
      '}' \
    > /tmp/cyberwave-cpp-consumer/main.cpp

RUN --mount=type=cache,target=/root/.ccache,sharing=locked \
    cmake -S /tmp/cyberwave-cpp-consumer -B /tmp/cyberwave-cpp-consumer/build \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_PREFIX_PATH=/usr/local && \
    cmake --build /tmp/cyberwave-cpp-consumer/build --parallel && \
    /tmp/cyberwave-cpp-consumer/build/cyberwave_cpp_install_validation

# Print cache statistics so they appear in the workflow log.
RUN --mount=type=cache,target=/root/.ccache,sharing=locked \
    echo "=== ccache stats (Dockerfile build) ===" && ccache --show-stats

CMD ["/bin/bash"]
