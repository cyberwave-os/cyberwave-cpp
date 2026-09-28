# Multi-stage Dockerfile for Cyberwave C++ SDK tests
# This Dockerfile builds the REST SDK library and runs tests against it

FROM ubuntu:22.04 AS builder

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
# Generated REST translation units are large enough to exhaust the small CI
# Docker VM when two compilers overlap. Keep the test image deterministic by
# default; callers with more memory can opt in to higher parallelism.
ARG CYBERWAVE_CPP_BUILD_JOBS=1

# Install build dependencies (REST + MQTT test)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libcpprest-dev \
    libssl-dev \
    libboost-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libboost-chrono-dev \
    libboost-filesystem-dev \
    libboost-random-dev \
    nlohmann-json3-dev \
    libmosquitto-dev \
    libspdlog-dev \
    pkg-config \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /sdk

# Copy SDK sources first so test-only edits keep the expensive REST build cached
# (build context: cyberwave-cpp).
COPY rest /sdk/rest
COPY mqtt /sdk/mqtt

# Build the REST SDK library
WORKDIR /sdk/rest
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j${CYBERWAVE_CPP_BUILD_JOBS} && \
    make install && \
    ldconfig

# Build the tests (REST + optional MQTT test)
COPY include /sdk/include
COPY tests /sdk/tests
WORKDIR /sdk/tests
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j${CYBERWAVE_CPP_BUILD_JOBS}

# Runtime stage - smaller image with just what's needed to run tests
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies (REST + MQTT test may need Paho at runtime)
RUN apt-get update && apt-get install -y \
    libcpprest2.10 \
    libssl3 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-chrono1.74.0 \
    libmosquitto1 \
    libspdlog1 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy the built library and tests from builder
COPY --from=builder /usr/local/lib/libCppRestOpenAPIClient.* /usr/local/lib/
COPY --from=builder /usr/local/include/CppRestOpenAPIClient /usr/local/include/CppRestOpenAPIClient
COPY --from=builder /sdk/tests/build /usr/local/cyberwave-tests

# Update library cache
RUN ldconfig

# Set default environment variables (can be overridden at runtime)
ENV CYBERWAVE_BASE_URL=http://host.docker.internal:8000

# Health check to verify backend is accessible
HEALTHCHECK --interval=5s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -f ${CYBERWAVE_BASE_URL}/api/v1/openapi.json || exit 1

# The test image installs all MQTT dependencies, so both binaries must pass.
CMD ["sh", "-c", "/usr/local/cyberwave-tests/test_sdk && /usr/local/cyberwave-tests/test_mqtt"]
