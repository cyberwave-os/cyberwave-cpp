# Top-level build: REST client, cyberwave SDK layer, and optional tests/examples.
cmake_minimum_required(VERSION 3.14)
project(CyberwaveCppSDK VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

option(CYBERWAVE_BUILD_TESTS "Build cyberwave C++ SDK tests" ON)
option(CYBERWAVE_BUILD_EXAMPLES "Build cyberwave C++ SDK examples" ON)
option(CYBERWAVE_ENABLE_WEBRTC "Enable WebRTC support (fetches LibDataChannel if not found)" ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rest/CMakeLists.txt")
    message(FATAL_ERROR
        "Generated REST sources are missing in ${CMAKE_CURRENT_SOURCE_DIR}/rest.\n"
        "Run ./install.sh (preferred) or ../cpp-sdk-gen.sh first.")
endif()

find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET libavcodec libavutil libswscale)
endif()

# REST client (generated)
add_subdirectory(rest)

# Suppress cpprestsdk's U() macro for all consumers of CppRestOpenAPIClient.
# Without this, U(x) leaks into downstream headers (e.g. fmt/core.h) where 'U'
# is used as a template parameter name, causing hard compilation errors on
# Ubuntu 24.04 (fmt 9 + GCC 13 + C++20).
target_compile_definitions(CppRestOpenAPIClient INTERFACE _TURN_OFF_PLATFORM_STRING)

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
    include/cyberwave/data.h
    include/cyberwave/workers.h
    include/cyberwave/manifest.h
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
    src/cyberwave/data.cpp
    src/cyberwave/workers.cpp
    src/cyberwave/manifest.cpp
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
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(cyberwave_sdk PUBLIC
    CppRestOpenAPIClient
    cpprestsdk::cpprest
    OpenSSL::SSL
    OpenSSL::Crypto
)
if(FFMPEG_FOUND)
    target_link_libraries(cyberwave_sdk PUBLIC PkgConfig::FFMPEG)
    target_compile_definitions(cyberwave_sdk PRIVATE CYBERWAVE_HAS_FFMPEG=1)
    set(CYBERWAVE_SDK_HAS_FFMPEG TRUE)
else()
    message(STATUS "FFmpeg development libraries not found — CameraStreamer will build without H264 encoding support.")
    set(CYBERWAVE_SDK_HAS_FFMPEG FALSE)
endif()

# WebRTC producer support (libdatachannel / rtc.hpp).
# Tries the system package first; fetches from source automatically when missing.
set(CYBERWAVE_SDK_HAS_WEBRTC FALSE)
if(CYBERWAVE_ENABLE_WEBRTC)
    set(CYBERWAVE_SDK_HAS_WEBRTC TRUE)
    find_package(LibDataChannel QUIET)
    if(LibDataChannel_FOUND)
        message(STATUS "Using system LibDataChannel")
        target_link_libraries(cyberwave_sdk PRIVATE LibDataChannel::LibDataChannel)
    else()
        message(STATUS "LibDataChannel not found on system — fetching v0.22.4 from source")
        FetchContent_Declare(
            libdatachannel
            GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
            GIT_TAG v0.22.4
        )
        set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
        set(NO_MEDIA OFF CACHE BOOL "" FORCE)
        set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
        set(NO_TESTS ON CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(libdatachannel)
        target_link_libraries(cyberwave_sdk PRIVATE datachannel)
    endif()
    target_compile_definitions(cyberwave_sdk PRIVATE CYBERWAVE_HAS_LIBDATACHANNEL=1)

    # camera_streaming.cpp uses spdlog/fmt for logging inside the WebRTC code path.
    # Define SPDLOG_HEADER_ONLY and FMT_HEADER_ONLY so that all spdlog and fmt
    # symbols are fully instantiated inside camera_streaming.cpp.o without linking
    # against a compiled libspdlog.so or libfmt.so.  This avoids exporting a
    # shared-library dependency into the installed CMake targets file, which would
    # break downstream consumers that don't have spdlog on their CMake search path.
    target_compile_definitions(cyberwave_sdk PRIVATE SPDLOG_HEADER_ONLY FMT_HEADER_ONLY)
endif()

# Install targets and package config so downstream users can find_package(CyberwaveCppSDK CONFIG REQUIRED).
install(
    TARGETS cyberwave_sdk
    EXPORT CyberwaveCppSDKTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(TARGET CppRestOpenAPIClient)
    install(
        TARGETS CppRestOpenAPIClient
        EXPORT CyberwaveCppSDKTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/cyberwave
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# cyberwave_mqtt_adapter.h includes this public header.
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/mqtt/mqtt_client.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cyberwave
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rest/include/CppRestOpenAPIClient")
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/rest/include/CppRestOpenAPIClient
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

set(CYBERWAVE_CMAKE_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/CyberwaveCppSDK")

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CyberwaveCppSDKConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CyberwaveCppSDKConfig.cmake
    INSTALL_DESTINATION ${CYBERWAVE_CMAKE_CONFIG_INSTALL_DIR}
)

# Stamp build-time feature flags so the config template can conditionally
# require optional dependencies only when they were part of the build.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CyberwaveCppSDKBuildConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CyberwaveCppSDKBuildConfig.cmake
    @ONLY
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CyberwaveCppSDKConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/CyberwaveCppSDKConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/CyberwaveCppSDKConfigVersion.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/CyberwaveCppSDKBuildConfig.cmake
    DESTINATION ${CYBERWAVE_CMAKE_CONFIG_INSTALL_DIR}
)

install(
    EXPORT CyberwaveCppSDKTargets
    FILE CyberwaveCppSDKTargets.cmake
    NAMESPACE CyberwaveCppSDK::
    DESTINATION ${CYBERWAVE_CMAKE_CONFIG_INSTALL_DIR}
)

if(CYBERWAVE_BUILD_TESTS)
    enable_testing()

    # All tests link cyberwave_sdk which transitively provides OpenSSL, cpprestsdk,
    # and rest/include — no need to repeat those dependencies per target.
    set(_CYBERWAVE_UNIT_TESTS
        test_config test_exceptions test_client test_alerts test_workspaces
        test_edges test_projects test_environments test_assets test_twins
        test_workflows test_scene test_data test_workers test_manifest
        test_utils test_twin_subclasses test_compact test_motion
        test_navigation test_keyboard test_device_pairing test_edge_node
        test_amr_edge_node test_camera_streaming test_depth_streaming
        test_mqtt_config test_twin test_mqtt_interface
    )
    foreach(_t IN LISTS _CYBERWAVE_UNIT_TESTS)
        add_executable(${_t} tests/${_t}.cpp)
        target_link_libraries(${_t} PRIVATE cyberwave_sdk)
        add_test(NAME ${_t} COMMAND ${_t})
    endforeach()

    # Integration test: exercises the generated REST client against a live backend.
    add_executable(test_sdk tests/test_sdk.cpp)
    target_link_libraries(test_sdk PRIVATE CppRestOpenAPIClient cpprestsdk::cpprest OpenSSL::SSL OpenSSL::Crypto)
    add_test(NAME test_sdk COMMAND test_sdk)
    set_tests_properties(test_sdk PROPERTIES LABELS integration)
endif()

if(CYBERWAVE_BUILD_EXAMPLES)
    # ---------------------------------------------------------------------------
    # SDK-level examples (mirror Python examples/)
    # These are not registered as ctest targets — they need a live backend /
    # MQTT broker. Set CYBERWAVE_API_KEY and CYBERWAVE_BASE_URL before running.
    # ---------------------------------------------------------------------------

    # REST-only examples: build unconditionally (no MQTT lib required).
    foreach(_ex quickstart compact alerts workflows capture_frame)
        add_executable(example_${_ex} examples/${_ex}.cpp)
        target_link_libraries(example_${_ex} PRIVATE cyberwave_sdk)
        set_target_properties(example_${_ex} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
    endforeach()

    # MQTT examples: only built when libmosquitto (and its deps) are available.
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND AND NOT TARGET PkgConfig::MOSQUITTO)
        pkg_check_modules(MOSQUITTO QUIET IMPORTED_TARGET libmosquitto)
    endif()
    find_package(nlohmann_json QUIET)
    find_package(spdlog QUIET)

    if(TARGET PkgConfig::MOSQUITTO AND nlohmann_json_FOUND AND spdlog_FOUND)
        message(STATUS "libmosquitto found — building MQTT examples (camera_stream, depth_stream, command_receiver_simple, go2_locomotion)")

        # Build the mqtt/ subdirectory to get the cyberwave_mqtt_client target.
        # EXCLUDE_FROM_ALL avoids polluting `make all` / `cmake --build`.
        add_subdirectory(mqtt EXCLUDE_FROM_ALL)

        foreach(_ex camera_stream depth_stream command_receiver_simple go2_locomotion)
            add_executable(example_${_ex} examples/${_ex}.cpp)
            # mqtt/ directory provides mqtt_client.h (needed by cyberwave_mqtt_adapter.h)
            target_include_directories(example_${_ex} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mqtt)
            target_link_libraries(example_${_ex} PRIVATE cyberwave_sdk cyberwave_mqtt_client)
            set_target_properties(example_${_ex} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
        endforeach()

        # quickstart can optionally auto-wire MQTT subscriptions when the MQTT
        # adapter is available.
        target_include_directories(example_quickstart PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mqtt)
        target_link_libraries(example_quickstart PRIVATE cyberwave_mqtt_client)
        target_compile_definitions(example_quickstart PRIVATE CYBERWAVE_HAVE_MQTT=1)

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
                ${OpenCV_LIBS}
            )
            set_target_properties(example_camera_stream_opencv PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
        else()
            message(STATUS "OpenCV not found — skipping MQTT example camera_stream_opencv.")
        endif()
    else()
        message(STATUS "libmosquitto not found — MQTT examples skipped (camera_stream, depth_stream, command_receiver_simple, go2_locomotion).")
        message(STATUS "  Install libmosquitto-dev, nlohmann-json and spdlog to build them.")
    endif()
endif()
