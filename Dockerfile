FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

ARG OPENAPI_URL=https://api.cyberwave.com/api/v1/openapi.json

# Compiler cache: this clean-room build routes through the SAME GCS-backed
# sccache the host CI job uses (see .github/actions/setup-sccache), so the
# Docker build reuses the objects the host compiled moments earlier instead of
# rebuilding from a cold, runner-local cache. When the GCS bucket or credentials
# are absent (e.g. a plain local `docker build`), sccache-start (below) skips the
# GCS backend and falls back to the on-disk cache mount.
ARG SCCACHE_VERSION=0.10.0
ARG SCCACHE_GCS_BUCKET=""
ARG SCCACHE_GCS_PREFIX="cyberwave-cpp/"

WORKDIR /opt/cyberwave-cpp

# Copy only install.sh first so the apt layer is cached across source changes.
# curl/ca-certificates back the sccache download below.
COPY install.sh /opt/cyberwave-cpp/install.sh
RUN ./install.sh --deps-only && \
    apt-get install -y --no-install-recommends curl ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Install sccache (static musl build, arch-aware) — same version as the host.
RUN set -eux; \
    case "$(uname -m)" in \
      x86_64)  triplet="x86_64-unknown-linux-musl"  ;; \
      aarch64) triplet="aarch64-unknown-linux-musl" ;; \
      *) echo "unsupported arch: $(uname -m)" >&2; exit 1 ;; \
    esac; \
    archive="sccache-v${SCCACHE_VERSION}-${triplet}.tar.gz"; \
    curl -fsSL "https://github.com/mozilla/sccache/releases/download/v${SCCACHE_VERSION}/${archive}" -o /tmp/sccache.tgz; \
    tar -C /tmp -xzf /tmp/sccache.tgz; \
    install -m 0755 "/tmp/sccache-v${SCCACHE_VERSION}-${triplet}/sccache" /usr/local/bin/sccache; \
    rm -rf /tmp/sccache.tgz "/tmp/sccache-v${SCCACHE_VERSION}-${triplet}"; \
    sccache --version

# sccache-start: enable the shared GCS backend ONLY when both a bucket and the
# mounted credentials file are present, otherwise use the local on-disk cache.
# This avoids the failure mode where a half-configured GCS backend (bucket set
# but no readable key) prevents the sccache server from starting at all — which
# surfaces later as "failed to spawn <compiler>: No such file or directory"
# (the missing server socket) on the first compile. Starting the server here
# also makes any backend error fail loudly and early instead of mid-build.
# The bucket/prefix are baked from build args; the key path is a constant.
RUN printf '%s\n' \
      '#!/bin/sh' \
      'set -eu' \
      ': "${SCCACHE_DIR:=/root/.cache/sccache}"' \
      'export SCCACHE_DIR' \
      "GCS_BUCKET=\"${SCCACHE_GCS_BUCKET}\"" \
      "GCS_PREFIX=\"${SCCACHE_GCS_PREFIX}\"" \
      'GCS_KEY_PATH=/run/secrets/gcs_creds' \
      'sccache --stop-server >/dev/null 2>&1 || true' \
      'if [ -n "$GCS_BUCKET" ] && [ -s "$GCS_KEY_PATH" ]; then' \
      '  export SCCACHE_GCS_BUCKET="$GCS_BUCKET"' \
      '  export SCCACHE_GCS_KEY_PREFIX="$GCS_PREFIX"' \
      '  export SCCACHE_GCS_KEY_PATH="$GCS_KEY_PATH"' \
      '  export SCCACHE_GCS_RW_MODE=READ_WRITE' \
      '  echo "[sccache] GCS backend enabled (bucket=$GCS_BUCKET prefix=$GCS_PREFIX)"' \
      'else' \
      '  echo "[sccache] No GCS bucket/credentials — using local disk cache ($SCCACHE_DIR)"' \
      'fi' \
      'sccache --start-server' \
      'exec "$@"' \
    > /usr/local/bin/sccache-start && chmod +x /usr/local/bin/sccache-start

# Route every cmake compile through sccache. install.sh honours a preset
# CMAKE_*_COMPILER_LAUNCHER, and the examples/consumer builds below pass it too.
ENV CMAKE_C_COMPILER_LAUNCHER=sccache \
    CMAKE_CXX_COMPILER_LAUNCHER=sccache \
    SCCACHE_DIR=/root/.cache/sccache

# Now copy the rest of the source tree.
COPY . /opt/cyberwave-cpp

# Build and install the SDK in a clean environment. The sccache dir mount
# (sharing=shared — sccache is multi-process safe, unlike the old locked ccache
# mount) backs the local fallback; the gcs_creds secret enables the shared GCS
# cache in CI and is optional so local builds still work without it.
RUN --mount=type=cache,target=/root/.cache/sccache,sharing=shared \
    --mount=type=secret,id=gcs_creds,required=false \
    sccache-start ./install.sh --skip-deps --openapi-url "${OPENAPI_URL}" --run-tests

# Build examples separately to verify they compile against the installed SDK.
RUN --mount=type=cache,target=/root/.cache/sccache,sharing=shared \
    --mount=type=secret,id=gcs_creds,required=false \
    sccache-start cmake -S examples -B /tmp/examples-build \
        -DCMAKE_C_COMPILER_LAUNCHER=sccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=sccache \
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

RUN --mount=type=cache,target=/root/.cache/sccache,sharing=shared \
    --mount=type=secret,id=gcs_creds,required=false \
    sccache-start cmake -S /tmp/cyberwave-cpp-consumer -B /tmp/cyberwave-cpp-consumer/build \
        -DCMAKE_C_COMPILER_LAUNCHER=sccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=sccache \
        -DCMAKE_PREFIX_PATH=/usr/local && \
    cmake --build /tmp/cyberwave-cpp-consumer/build --parallel && \
    /tmp/cyberwave-cpp-consumer/build/cyberwave_cpp_install_validation

# Print cache statistics so they appear in the workflow log.
RUN --mount=type=cache,target=/root/.cache/sccache,sharing=shared \
    --mount=type=secret,id=gcs_creds,required=false \
    sccache-start sh -c 'echo "=== sccache stats (Dockerfile build) ===" && sccache --show-stats'

CMD ["/bin/bash"]
