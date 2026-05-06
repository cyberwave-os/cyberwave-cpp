# Multi-stage Dockerfile for Cyberwave C++ SDK tests
# This Dockerfile builds the REST SDK library and runs tests against it

FROM ubuntu:22.04 AS builder

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

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

# Copy REST SDK, MQTT client, and tests (build context: cyberwave-cpp)
COPY rest /sdk/rest
COPY mqtt /sdk/mqtt
COPY tests /sdk/tests

# Build the REST SDK library
WORKDIR /sdk/rest
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Build the tests (REST + optional MQTT test)
WORKDIR /sdk/tests
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

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
ENV CYBERWAVE_API_KEY=test-api-key

# Health check to verify backend is accessible
HEALTHCHECK --interval=5s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -f ${CYBERWAVE_BASE_URL}/api/v1/openapi.json || exit 1

# Run REST test then MQTT test if built (MQTT test skips gracefully if broker unavailable)
CMD ["sh", "-c", "/usr/local/cyberwave-tests/test_sdk && (/usr/local/cyberwave-tests/test_mqtt 2>/dev/null || true)"]

