cmake_minimum_required(VERSION 3.14)
project(cyberwave_mqtt_client VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages.
find_package(PkgConfig REQUIRED)
if(NOT TARGET PkgConfig::MOSQUITTO)
  pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
  find_package(nlohmann_json REQUIRED)
endif()
if(NOT TARGET spdlog::spdlog)
  find_package(spdlog REQUIRED)
endif()

# libmosquitto 2.1+ exposes cJSON in public headers on some builds (Homebrew macOS).
pkg_check_modules(CJSON QUIET IMPORTED_TARGET libcjson)
if(NOT CJSON_FOUND)
  pkg_check_modules(CJSON QUIET IMPORTED_TARGET cjson)
endif()

## Build a static mqtt client library from mqtt_client.cpp so examples can
## link against a real implementation.
add_library(cyberwave_mqtt_client STATIC
    mqtt_client.cpp
)

# Include directories (parent include/ for cyberwave/constants.h used by constants.h)
target_include_directories(cyberwave_mqtt_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

# Place the static archive in the source-level `lib/` folder (e.g. mqtt/lib)
set_target_properties(cyberwave_mqtt_client PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# Link libraries
target_link_libraries(cyberwave_mqtt_client
    PUBLIC
        PkgConfig::MOSQUITTO
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)
if(CJSON_FOUND)
    target_link_libraries(cyberwave_mqtt_client PUBLIC PkgConfig::CJSON)
else()
    find_path(CJSON_INCLUDE_DIR NAMES cjson/cJSON.h)
    if(CJSON_INCLUDE_DIR)
        target_include_directories(cyberwave_mqtt_client PUBLIC ${CJSON_INCLUDE_DIR})
    else()
        message(WARNING "cJSON headers not found — mqtt_client may fail to compile against Mosquitto 2.x")
    endif()
endif()

# Installation rules
install(TARGETS cyberwave_mqtt_client
    EXPORT cyberwave_mqtt_client-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers and export
install(FILES mqtt_client.h
    DESTINATION include/cyberwave
)

install(EXPORT cyberwave_mqtt_client-targets
    NAMESPACE cyberwave::
    DESTINATION lib/cmake/cyberwave_mqtt_client
)

# Generate a config file that re-creates transitive dependencies for consumers.
include(CMakePackageConfigHelpers)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cyberwave_mqtt_client-config.cmake"
[=[
include(CMakeFindDependencyMacro)
find_dependency(PkgConfig REQUIRED)
if(NOT TARGET PkgConfig::MOSQUITTO)
  pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)
endif()
find_dependency(nlohmann_json REQUIRED)
find_dependency(spdlog REQUIRED)
if(NOT TARGET PkgConfig::CJSON)
  pkg_check_modules(CJSON QUIET IMPORTED_TARGET libcjson)
  if(NOT CJSON_FOUND)
    pkg_check_modules(CJSON QUIET IMPORTED_TARGET cjson)
  endif()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/cyberwave_mqtt_client-targets.cmake")
]=]
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cyberwave_mqtt_client-config.cmake"
    DESTINATION lib/cmake/cyberwave_mqtt_client
)
