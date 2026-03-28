FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

ARG OPENAPI_URL=https://api.cyberwave.com/api/v1/openapi.json

WORKDIR /opt/cyberwave-cpp

COPY . /opt/cyberwave-cpp

# Build and install the SDK in a clean environment (tests built but not run here).
RUN ./install.sh --openapi-url "${OPENAPI_URL}" --run-tests

# Build examples separately to verify they compile against the installed SDK.
RUN cmake -S examples -B /tmp/examples-build \
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

RUN cmake -S /tmp/cyberwave-cpp-consumer -B /tmp/cyberwave-cpp-consumer/build \
        -DCMAKE_PREFIX_PATH=/usr/local && \
    cmake --build /tmp/cyberwave-cpp-consumer/build --parallel && \
    /tmp/cyberwave-cpp-consumer/build/cyberwave_cpp_install_validation

CMD ["/bin/bash"]
