#!/bin/bash
# Helper script to run C++ SDK tests in Docker
# Usage: ./run-tests.sh [OPTIONS]
#
# Options:
#   --rebuild           Rebuild the Docker image (use --no-cache)
#   --base-url URL      Backend URL (default: http://localhost:8000)
#   --token TOKEN       API token (default: test-api-key)
#   --help              Show this help message

set -e

# Colors for output
COLOR_GREEN="\033[32m"
COLOR_RED="\033[31m"
COLOR_YELLOW="\033[33m"
COLOR_BLUE="\033[34m"
COLOR_RESET="\033[0m"

# Default values
REBUILD=false
BASE_URL="http://host.docker.internal:8000"
TOKEN="test-api-key"

# Show usage
show_usage() {
    echo "Usage: ./run-tests.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --rebuild           Rebuild the Docker image (use --no-cache)"
    echo "  --base-url URL      Backend URL (default: http://localhost:8000)"
    echo "  --token TOKEN       API token (default: test-api-key)"
    echo "  --help              Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./run-tests.sh"
    echo "  ./run-tests.sh --rebuild"
    echo "  ./run-tests.sh --base-url http://172.17.0.1:8000"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --rebuild)
            REBUILD=true
            shift
            ;;
        --base-url)
            BASE_URL="$2"
            shift 2
            ;;
        --token)
            TOKEN="$2"
            shift 2
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            echo -e "${COLOR_RED}❌ Error: Unknown option: $1${COLOR_RESET}"
            echo ""
            show_usage
            exit 1
            ;;
    esac
done

echo -e "${COLOR_BLUE}╔═══════════════════════════════════════════════════════════╗${COLOR_RESET}"
echo -e "${COLOR_BLUE}║  Cyberwave C++ SDK Test Runner                           ║${COLOR_RESET}"
echo -e "${COLOR_BLUE}╚═══════════════════════════════════════════════════════════╝${COLOR_RESET}"
echo ""

# Check if we're in the right directory
if [ ! -f "test_sdk.cpp" ]; then
    echo -e "${COLOR_RED}❌ Error: Must be run from cyberwave-cpp-sdk/tests directory${COLOR_RESET}"
    exit 1
fi

# Check if REST SDK exists
if [ ! -d "../rest" ]; then
    echo -e "${COLOR_RED}❌ Error: REST SDK not found at ../rest${COLOR_RESET}"
    echo -e "${COLOR_YELLOW}ℹ️  Run ./cpp-sdk-gen.sh from cyberwave-sdks directory first${COLOR_RESET}"
    exit 1
fi

# Check if backend is accessible
echo -e "${COLOR_YELLOW}ℹ️  Checking backend connectivity...${COLOR_RESET}"
if ! curl -s -f "${BASE_URL}/api/v1/openapi.json" > /dev/null 2>&1; then
    echo -e "${COLOR_RED}❌ Error: Backend not accessible at ${BASE_URL}${COLOR_RESET}"
    echo -e "${COLOR_YELLOW}ℹ️  Make sure the backend is running:${COLOR_RESET}"
    echo -e "   cd cyberwave-backend"
    echo -e "   docker compose -f local.yml up -d"
    exit 1
fi
echo -e "${COLOR_GREEN}✅ Backend is accessible${COLOR_RESET}"
echo ""

# Build Docker image
cd ..
if [ "$REBUILD" = true ]; then
    echo -e "${COLOR_YELLOW}ℹ️  Rebuilding Docker image (no cache)...${COLOR_RESET}"
    docker build --no-cache -f tests/Dockerfile -t cyberwave-cpp-tests .
else
    echo -e "${COLOR_YELLOW}ℹ️  Building Docker image...${COLOR_RESET}"
    docker build -f tests/Dockerfile -t cyberwave-cpp-tests .
fi

echo ""
echo -e "${COLOR_BLUE}╔═══════════════════════════════════════════════════════════╗${COLOR_RESET}"
echo -e "${COLOR_BLUE}║  Running Tests                                            ║${COLOR_RESET}"
echo -e "${COLOR_BLUE}╚═══════════════════════════════════════════════════════════╝${COLOR_RESET}"
echo ""
echo -e "${COLOR_YELLOW}Configuration:${COLOR_RESET}"
echo -e "  Base URL: ${BASE_URL}"
echo -e "  Token:    ${TOKEN:0:8}..."
echo ""

# Run tests (REST + MQTT connect/subscribe/disconnect; MQTT skips if broker unreachable)
# Note: On Linux, you may need to add --add-host=host.docker.internal:172.17.0.1
# Optional: -e CYBERWAVE_MQTT_HOST=host.docker.internal -e CYBERWAVE_MQTT_PORT=1883 for MQTT test
docker run --rm \
  -e CYBERWAVE_BASE_URL="${BASE_URL}" \
  -e CYBERWAVE_API_KEY="${TOKEN}" \
  -e CYBERWAVE_MQTT_HOST="${CYBERWAVE_MQTT_HOST:-host.docker.internal}" \
  -e CYBERWAVE_MQTT_PORT="${CYBERWAVE_MQTT_PORT:-1883}" \
  cyberwave-cpp-tests

echo ""
echo -e "${COLOR_GREEN}✅ Test run complete!${COLOR_RESET}"

