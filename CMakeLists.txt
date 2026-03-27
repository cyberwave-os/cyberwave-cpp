# Top-level build: REST client, cyberwave SDK layer, and symmetric tests.
cmake_minimum_required(VERSION 3.10)
project(cyberwave-cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# REST client (generated)
add_subdirectory(rest)

# Hand-crafted SDK layer (config, exceptions, client, twin, alerts, workspaces, edges)
set(CYBERWAVE_SDK_HEADERS
    include/cyberwave/config.h
    include/cyberwave/exceptions.h
    include/cyberwave/client.h
    include/cyberwave/twin.h
    include/cyberwave/alerts.h
    include/cyberwave/workspaces.h
    include/cyberwave/edges.h
    include/cyberwave/projects.h
    include/cyberwave/environments.h
    include/cyberwave/assets.h
    include/cyberwave/twins.h
    include/cyberwave/workflows.h
    include/cyberwave/scene.h
    include/cyberwave/navigation.h
    include/cyberwave/motion.h
    include/cyberwave/joints.h
    include/cyberwave/constants.h
    include/cyberwave/utils.h
    include/cyberwave/twin_subclasses.h
    include/cyberwave/compact.h
    include/cyberwave/keyboard.h
    include/cyberwave/mqtt_interface.h
    include/cyberwave/cyberwave_mqtt_client_adapter.h
    include/cyberwave/device_pairing.h
    include/cyberwave/edge/edge_config.h
    include/cyberwave/edge/base_edge_node.h
    include/cyberwave/edge/amr_types.h
    include/cyberwave/edge/amr_adapter.h
    include/cyberwave/edge/amr_edge_node.h
    include/cyberwave/camera_streaming.h
)
set(CYBERWAVE_SDK_SOURCES
    src/cyberwave/config.cpp
    src/cyberwave/exceptions.cpp
    src/cyberwave/client.cpp
    src/cyberwave/twin.cpp
    src/cyberwave/alerts.cpp
    src/cyberwave/workspaces.cpp
    src/cyberwave/edges.cpp
    src/cyberwave/projects.cpp
    src/cyberwave/environments.cpp
    src/cyberwave/assets.cpp
    src/cyberwave/twins.cpp
    src/cyberwave/workflows.cpp
    src/cyberwave/scene.cpp
    src/cyberwave/navigation.cpp
    src/cyberwave/motion.cpp
    src/cyberwave/joints.cpp
    src/cyberwave/utils.cpp
    src/cyberwave/twin_subclasses.cpp
    src/cyberwave/compact.cpp
    src/cyberwave/keyboard.cpp
    src/cyberwave/device_pairing.cpp
    src/cyberwave/edge/edge_config.cpp
    src/cyberwave/edge/base_edge_node.cpp
    src/cyberwave/edge/amr_types.cpp
    src/cyberwave/edge/amr_edge_node.cpp
    src/cyberwave/camera_streaming.cpp
)
add_library(cyberwave_sdk ${CYBERWAVE_SDK_HEADERS} ${CYBERWAVE_SDK_SOURCES})
target_include_directories(cyberwave_sdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rest/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(cyberwave_sdk PUBLIC CppRestOpenAPIClient OpenSSL::SSL OpenSSL::Crypto)
# Match rest build: string_t = std::string
target_compile_definitions(cyberwave_sdk PUBLIC _TURN_OFF_PLATFORM_STRING)

# WebRTC producer support (libdatachannel / rtc.hpp)
find_package(LibDataChannel QUIET)
if(LibDataChannel_FOUND)
  target_link_libraries(cyberwave_sdk PRIVATE LibDataChannel::LibDataChannel)
else()
  include(FetchContent)
  FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.22.4
  )
  set(NO_WEBSOCKET ON CACHE BOOL "Disable websocket transport" FORCE)
  set(NO_MEDIA OFF CACHE BOOL "Enable media support for WebRTC video track" FORCE)
  set(NO_EXAMPLES ON CACHE BOOL "Disable libdatachannel examples" FORCE)
  set(NO_TESTS ON CACHE BOOL "Disable libdatachannel tests" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "Prefer static linkage for container runtime" FORCE)
  FetchContent_MakeAvailable(libdatachannel)
  target_link_libraries(cyberwave_sdk PRIVATE datachannel)
endif()

# Tests (symmetric with Python: config, exceptions, client, alerts)
enable_testing()
find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(test_config tests/test_config.cpp)
target_link_libraries(test_config PRIVATE cyberwave_sdk)
add_test(NAME test_config COMMAND test_config)

add_executable(test_exceptions tests/test_exceptions.cpp)
target_link_libraries(test_exceptions PRIVATE cyberwave_sdk)
add_test(NAME test_exceptions COMMAND test_exceptions)

add_executable(test_client tests/test_client.cpp)
target_link_libraries(test_client PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_client COMMAND test_client)

add_executable(test_alerts tests/test_alerts.cpp)
target_include_directories(test_alerts PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_alerts PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_alerts COMMAND test_alerts)

add_executable(test_workspaces tests/test_workspaces.cpp)
target_include_directories(test_workspaces PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_workspaces PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_workspaces COMMAND test_workspaces)

add_executable(test_edges tests/test_edges.cpp)
target_include_directories(test_edges PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_edges PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_edges COMMAND test_edges)

add_executable(test_projects tests/test_projects.cpp)
target_include_directories(test_projects PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_projects PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_projects COMMAND test_projects)

add_executable(test_environments tests/test_environments.cpp)
target_include_directories(test_environments PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_environments PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_environments COMMAND test_environments)

add_executable(test_assets tests/test_assets.cpp)
target_include_directories(test_assets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_assets PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_assets COMMAND test_assets)

add_executable(test_twins tests/test_twins.cpp)
target_include_directories(test_twins PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_twins PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_twins COMMAND test_twins)

add_executable(test_workflows tests/test_workflows.cpp)
target_include_directories(test_workflows PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_workflows PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_workflows COMMAND test_workflows)

add_executable(test_scene tests/test_scene.cpp)
target_include_directories(test_scene PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_scene PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_scene COMMAND test_scene)

add_executable(test_utils tests/test_utils.cpp)
target_link_libraries(test_utils PRIVATE cyberwave_sdk)
add_test(NAME test_utils COMMAND test_utils)

add_executable(test_twin_subclasses tests/test_twin_subclasses.cpp)
target_link_libraries(test_twin_subclasses PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_twin_subclasses COMMAND test_twin_subclasses)

add_executable(test_compact tests/test_compact.cpp)
target_link_libraries(test_compact PRIVATE cyberwave_sdk)
add_test(NAME test_compact COMMAND test_compact)

add_executable(test_motion tests/test_motion.cpp)
target_link_libraries(test_motion PRIVATE cyberwave_sdk)
add_test(NAME test_motion COMMAND test_motion)

add_executable(test_navigation tests/test_navigation.cpp)
target_link_libraries(test_navigation PRIVATE cyberwave_sdk)
add_test(NAME test_navigation COMMAND test_navigation)

add_executable(test_keyboard tests/test_keyboard.cpp)
target_link_libraries(test_keyboard PRIVATE cyberwave_sdk)
add_test(NAME test_keyboard COMMAND test_keyboard)

add_executable(test_device_pairing tests/test_device_pairing.cpp)
target_link_libraries(test_device_pairing PRIVATE cyberwave_sdk)
add_test(NAME test_device_pairing COMMAND test_device_pairing)

add_executable(test_edge_node tests/test_edge_node.cpp)
target_link_libraries(test_edge_node PRIVATE cyberwave_sdk)
add_test(NAME test_edge_node COMMAND test_edge_node)

add_executable(test_amr_edge_node tests/test_amr_edge_node.cpp)
target_link_libraries(test_amr_edge_node PRIVATE cyberwave_sdk)
add_test(NAME test_amr_edge_node COMMAND test_amr_edge_node)

add_executable(test_camera_streaming tests/test_camera_streaming.cpp)
target_link_libraries(test_camera_streaming PRIVATE cyberwave_sdk)
add_test(NAME test_camera_streaming COMMAND test_camera_streaming)

add_executable(test_depth_streaming tests/test_depth_streaming.cpp)
target_link_libraries(test_depth_streaming PRIVATE cyberwave_sdk)
add_test(NAME test_depth_streaming COMMAND test_depth_streaming)

add_executable(test_mqtt_config tests/test_mqtt_config.cpp)
target_link_libraries(test_mqtt_config PRIVATE cyberwave_sdk)
add_test(NAME test_mqtt_config COMMAND test_mqtt_config)

add_executable(test_twin tests/test_twin.cpp)
target_include_directories(test_twin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_link_libraries(test_twin PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_twin COMMAND test_twin)

add_executable(test_mqtt_interface tests/test_mqtt_interface.cpp)
target_link_libraries(test_mqtt_interface PRIVATE cyberwave_sdk)
add_test(NAME test_mqtt_interface COMMAND test_mqtt_interface)

# Optional: existing SDK test (links generated client only)
add_executable(test_sdk tests/test_sdk.cpp)
target_include_directories(test_sdk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rest/include)
target_compile_definitions(test_sdk PRIVATE _TURN_OFF_PLATFORM_STRING)
target_link_libraries(test_sdk PRIVATE CppRestOpenAPIClient cpprestsdk::cpprest OpenSSL::SSL OpenSSL::Crypto)
add_test(NAME test_sdk COMMAND test_sdk)

# ---------------------------------------------------------------------------
# SDK-level examples (mirror Python examples/)
# These are not registered as ctest targets — they need a live backend /
# MQTT broker. Set CYBERWAVE_API_KEY and CYBERWAVE_BASE_URL before running.
# ---------------------------------------------------------------------------

# REST-only examples: build unconditionally (no Paho required).
foreach(_ex quickstart compact alerts workflows capture_frame)
    add_executable(example_${_ex} examples/${_ex}.cpp)
    target_link_libraries(example_${_ex} PRIVATE cyberwave_sdk OpenSSL::SSL OpenSSL::Crypto)
    set_target_properties(example_${_ex} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
endforeach()

# MQTT examples: only built when PahoMqttCpp (and its deps) are available.
# They use PahoMqttAdapter to wire the real Paho MQTT client into the SDK.
find_package(PahoMqttCpp QUIET)
find_package(nlohmann_json QUIET)
find_package(spdlog QUIET)

if(PahoMqttCpp_FOUND AND nlohmann_json_FOUND AND spdlog_FOUND)
    message(STATUS "PahoMqttCpp found — building MQTT examples (camera_stream, depth_stream, command_receiver_simple, go2_locomotion)")

    # Build the mqtt/ subdirectory to get the cyberwave_mqtt_client target.
    # EXCLUDE_FROM_ALL avoids polluting `make all` / `cmake --build`.
    add_subdirectory(mqtt EXCLUDE_FROM_ALL)

    foreach(_ex camera_stream depth_stream command_receiver_simple go2_locomotion)
        add_executable(example_${_ex} examples/${_ex}.cpp)
        # mqtt/ directory provides mqtt_client.h (needed by paho_mqtt_adapter.h)
        target_include_directories(example_${_ex} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mqtt)
        target_link_libraries(example_${_ex} PRIVATE cyberwave_sdk cyberwave_mqtt_client OpenSSL::SSL OpenSSL::Crypto)
        set_target_properties(example_${_ex} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
    endforeach()

    # quickstart can optionally auto-wire MQTT subscriptions when the Paho-backed
    # adapter is available.
    target_include_directories(example_quickstart PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mqtt)
    target_link_libraries(example_quickstart PRIVATE cyberwave_mqtt_client)
    target_compile_definitions(example_quickstart PRIVATE CYBERWAVE_HAVE_PAHO_MQTT=1)

    # Optional OpenCV frame-source example for the raw VideoFrame contract.
    find_package(OpenCV QUIET COMPONENTS core imgproc videoio)
    if(OpenCV_FOUND)
        add_executable(example_camera_stream_opencv examples/camera_stream_opencv.cpp)
        target_include_directories(example_camera_stream_opencv PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/mqtt
            ${OpenCV_INCLUDE_DIRS}
        )
        target_link_libraries(example_camera_stream_opencv PRIVATE
            cyberwave_sdk
            cyberwave_mqtt_client
            OpenSSL::SSL
            OpenSSL::Crypto
            ${OpenCV_LIBS}
        )
        set_target_properties(example_camera_stream_opencv PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
    else()
        message(STATUS "OpenCV not found — skipping MQTT example camera_stream_opencv.")
    endif()
else()
    message(STATUS "PahoMqttCpp not found — MQTT examples skipped (camera_stream, depth_stream, command_receiver_simple, go2_locomotion).")
    message(STATUS "  Install paho-mqtt-cpp, nlohmann-json and spdlog to build them.")
endif()
