#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

INSTALL_PREFIX="/usr/local"
BUILD_DIR="${SCRIPT_DIR}/build/install"
BUILD_TYPE="Release"
OPENAPI_URL="${CYBERWAVE_OPENAPI_URL:-https://api.cyberwave.com/api/v1/openapi.json}"
OPENAPI_GENERATOR_VERSION="${CYBERWAVE_OPENAPI_GENERATOR_VERSION:-7.8.0}"

SKIP_DEPS=0
SKIP_GENERATE_REST=0
FORCE_GENERATE_REST=0
WITH_OPENCV=0
RUN_TESTS=0

usage() {
    cat <<'EOF'
Usage: ./install.sh [options]

Build and install Cyberwave C++ SDK system-wide (default: /usr/local).

Options:
  --prefix <path>                    Install prefix (default: /usr/local)
  --build-dir <path>                 Build directory (default: ./build/install)
  --build-type <type>                CMake build type (default: Release)
  --openapi-url <url>                OpenAPI URL to generate REST client when needed
  --openapi-generator-version <ver>  openapi-generator-cli version (default: 7.8.0)
  --skip-deps                        Skip dependency installation
  --skip-generate-rest               Do not auto-generate rest/ when missing
  --force-generate-rest              Regenerate rest/ even if it already exists
  --with-opencv                       Install OpenCV dev libs (for camera_stream_opencv example)
  --run-tests                        Run ctest after build
  -h, --help                         Show this help

Examples:
  ./install.sh
  ./install.sh --prefix /opt/cyberwave
  ./install.sh --force-generate-rest --openapi-url http://localhost:8000/api/v1/openapi.json
EOF
}

log() {
    echo "[install.sh] $*"
}

warn() {
    echo "[install.sh] WARNING: $*" >&2
}

err() {
    echo "[install.sh] ERROR: $*" >&2
}

have_cmd() {
    command -v "$1" >/dev/null 2>&1
}

run_as_root() {
    if [[ "$(id -u)" -eq 0 ]]; then
        "$@"
        return
    fi

    if have_cmd sudo; then
        sudo "$@"
        return
    fi

    err "Root privileges are required to run: $*"
    err "Re-run as root or install sudo."
    exit 1
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --prefix)
                INSTALL_PREFIX="$2"
                shift 2
                ;;
            --build-dir)
                BUILD_DIR="$2"
                shift 2
                ;;
            --build-type)
                BUILD_TYPE="$2"
                shift 2
                ;;
            --openapi-url)
                OPENAPI_URL="$2"
                shift 2
                ;;
            --openapi-generator-version)
                OPENAPI_GENERATOR_VERSION="$2"
                shift 2
                ;;
            --skip-deps)
                SKIP_DEPS=1
                shift
                ;;
            --skip-generate-rest)
                SKIP_GENERATE_REST=1
                shift
                ;;
            --force-generate-rest)
                FORCE_GENERATE_REST=1
                shift
                ;;
            --with-opencv)
                WITH_OPENCV=1
                shift
                ;;
            --run-tests)
                RUN_TESTS=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                err "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
}

install_deps_debian() {
    log "Installing system dependencies via apt-get"
    run_as_root apt-get update
    run_as_root apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        ca-certificates \
        curl \
        git \
        openjdk-17-jre-headless \
        libcpprest-dev \
        libssl-dev \
        libboost-all-dev \
        nlohmann-json3-dev \
        libpaho-mqtt-dev \
        libpaho-mqttpp-dev \
        libspdlog-dev

    if [[ "$WITH_OPENCV" -eq 1 ]]; then
        log "Installing OpenCV dev libraries (--with-opencv)"
        run_as_root apt-get install -y --no-install-recommends \
            libopencv-dev
    fi
}

install_deps() {
    if [[ "$SKIP_DEPS" -eq 1 ]]; then
        log "Skipping dependency installation (--skip-deps)"
        return
    fi

    if have_cmd apt-get; then
        install_deps_debian
        return
    fi

    err "Unsupported package manager. install.sh currently supports Debian/Ubuntu (apt-get)."
    err "Install dependencies manually, then re-run with --skip-deps."
    exit 1
}

download_openapi_generator_jar() {
    local cache_dir jar_path url
    cache_dir="${SCRIPT_DIR}/.cache"
    jar_path="${cache_dir}/openapi-generator-cli-${OPENAPI_GENERATOR_VERSION}.jar"
    url="https://repo1.maven.org/maven2/org/openapitools/openapi-generator-cli/${OPENAPI_GENERATOR_VERSION}/openapi-generator-cli-${OPENAPI_GENERATOR_VERSION}.jar"

    mkdir -p "${cache_dir}"
    if [[ -f "${jar_path}" ]]; then
        log "Using cached openapi-generator-cli ${OPENAPI_GENERATOR_VERSION}"
    else
        log "Downloading openapi-generator-cli ${OPENAPI_GENERATOR_VERSION}"
        curl -fsSL "${url}" -o "${jar_path}"
    fi

    echo "${jar_path}"
}

apply_openapi_patches() {
    local api_client_h api_client_cpp
    api_client_h="${SCRIPT_DIR}/rest/include/CppRestOpenAPIClient/ApiClient.h"
    api_client_cpp="${SCRIPT_DIR}/rest/src/ApiClient.cpp"

    if [[ ! -f "${api_client_h}" || ! -f "${api_client_cpp}" ]]; then
        err "Cannot apply OpenAPI patches; expected files were not found."
        exit 1
    fi

    python3 - <<PY
from pathlib import Path
import re
import sys

h_path = Path("${api_client_h}")
cpp_path = Path("${api_client_cpp}")

h_text = h_path.read_text()
decl = "static utility::string_t parameterToString(const HttpContent& value);"
needle = "static utility::string_t parameterToString(const ModelBase& value);"
if decl not in h_text:
    if needle not in h_text:
        print("Missing ModelBase declaration in ApiClient.h", file=sys.stderr)
        sys.exit(1)
    h_text = h_text.replace(needle, needle + "\\n    " + decl, 1)
    h_path.write_text(h_text)

cpp_text = cpp_path.read_text()
impl_signature = "utility::string_t ApiClient::parameterToString(const HttpContent& value)"
if impl_signature not in cpp_text:
    pattern = re.compile(
        r"(utility::string_t\\s+ApiClient::parameterToString\\(const ModelBase& value\\)\\s*\\{.*?^\\})",
        re.MULTILINE | re.DOTALL,
    )
    match = pattern.search(cpp_text)
    if not match:
        print("Missing ModelBase implementation in ApiClient.cpp", file=sys.stderr)
        sys.exit(1)
    block = match.group(1)
    addition = (
        "\\n\\nutility::string_t ApiClient::parameterToString(const HttpContent& value)\\n"
        "{\\n"
        "    return value.getFileName();\\n"
        "}\\n"
    )
    cpp_text = cpp_text.replace(block, block + addition, 1)
    cpp_path.write_text(cpp_text)
PY
}

generate_rest_sources() {
    local jar_path tmp_dir backup_dir
    tmp_dir="${SCRIPT_DIR}/rest-tmp"
    backup_dir="${SCRIPT_DIR}/.cache/install-backup"

    if ! have_cmd java; then
        err "Java runtime not found. Install Java or run without --skip-deps."
        exit 1
    fi

    jar_path="$(download_openapi_generator_jar)"

    log "Generating REST client from ${OPENAPI_URL}"
    rm -rf "${tmp_dir}"
    mkdir -p "${tmp_dir}"

    java -jar "${jar_path}" generate \
        -i "${OPENAPI_URL}" \
        -g cpp-restsdk \
        -o "${tmp_dir}" \
        --skip-validate-spec

    mkdir -p "${backup_dir}"
    cp "${SCRIPT_DIR}/rest/README.md" "${backup_dir}/rest-README.md" 2>/dev/null || true

    rm -rf "${SCRIPT_DIR}/rest"
    mkdir -p "${SCRIPT_DIR}/rest"
    cp -r "${tmp_dir}/." "${SCRIPT_DIR}/rest/"

    cp "${backup_dir}/rest-README.md" "${SCRIPT_DIR}/rest/README.md" 2>/dev/null || true
    rm -rf "${tmp_dir}"

    apply_openapi_patches
    log "REST sources generated at ${SCRIPT_DIR}/rest"
}

ensure_rest_sources() {
    local rest_cmake
    rest_cmake="${SCRIPT_DIR}/rest/CMakeLists.txt"

    if [[ "${FORCE_GENERATE_REST}" -eq 1 ]]; then
        if [[ "${SKIP_GENERATE_REST}" -eq 1 ]]; then
            err "--force-generate-rest and --skip-generate-rest are mutually exclusive."
            exit 1
        fi
        generate_rest_sources
        return
    fi

    if [[ -f "${rest_cmake}" ]]; then
        log "Using existing rest/ sources"
        return
    fi

    if [[ "${SKIP_GENERATE_REST}" -eq 1 ]]; then
        err "rest/ is missing and auto-generation is disabled."
        err "Run without --skip-generate-rest or generate rest/ manually."
        exit 1
    fi

    generate_rest_sources
}

configure_build_install() {
    local build_tests="OFF"
    [[ "${RUN_TESTS}" -eq 1 ]] && build_tests="ON"

    log "Configuring CMake build"
    cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DCYBERWAVE_BUILD_TESTS="${build_tests}" \
        -DCYBERWAVE_BUILD_EXAMPLES=OFF

    log "Building SDK"
    local jobs
    jobs="${CYBERWAVE_BUILD_JOBS:-$(( $(nproc) - 1 ))}"
    [[ "$jobs" -lt 1 ]] && jobs=1
    cmake --build "${BUILD_DIR}" --parallel "$jobs"

    if [[ "${RUN_TESTS}" -eq 1 ]]; then
        log "Running tests (ctest)"
        if ! ctest --test-dir "${BUILD_DIR}" --output-on-failure; then
            warn "Some tests failed. If test_sdk failed, this is expected when no backend is running on localhost:8000."
        fi
    fi

    log "Installing SDK to ${INSTALL_PREFIX}"
    if [[ "$(id -u)" -eq 0 ]]; then
        cmake --install "${BUILD_DIR}"
        if have_cmd ldconfig; then
            ldconfig || true
        fi
        return
    fi

    if [[ -w "${INSTALL_PREFIX}" ]]; then
        cmake --install "${BUILD_DIR}"
    elif have_cmd sudo; then
        sudo cmake --install "${BUILD_DIR}"
        if have_cmd ldconfig; then
            sudo ldconfig || true
        fi
    else
        err "No permissions for ${INSTALL_PREFIX} and sudo is unavailable."
        err "Use --prefix to choose a writable location, or run as root."
        exit 1
    fi
}

main() {
    parse_args "$@"

    log "Starting Cyberwave C++ SDK installation"
    log "Install prefix: ${INSTALL_PREFIX}"
    log "Build directory: ${BUILD_DIR}"
    log "Build type: ${BUILD_TYPE}"
    log "OpenAPI URL: ${OPENAPI_URL}"

    install_deps
    ensure_rest_sources
    configure_build_install

    log "Installation complete."
    log "Downstream projects can now use:"
    log "  find_package(CyberwaveCppSDK CONFIG REQUIRED)"
    log "  target_link_libraries(<target> PRIVATE CyberwaveCppSDK::cyberwave_sdk)"
}

main "$@"
