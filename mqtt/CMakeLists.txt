cmake_minimum_required(VERSION 3.14)
project(cyberwave_mqtt_client VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages. Parent may have already found them (to decide if MQTT
# examples are built); avoid find_package again to prevent duplicate imported
# target "PahoMqttC::PahoMqttC" (PahoMqttCpp's find_dependency(PahoMqttC) would
# re-run FindPahoMqttC.cmake and try to add the same target).
if(NOT TARGET PahoMqttCpp::paho-mqttpp3)
  find_package(PahoMqttCpp REQUIRED)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
  find_package(nlohmann_json REQUIRED)
endif()
if(NOT TARGET spdlog::spdlog)
  find_package(spdlog REQUIRED)
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
        PahoMqttCpp::paho-mqttpp3
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

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
    FILE cyberwave_mqtt_client-config.cmake
    NAMESPACE cyberwave::
    DESTINATION lib/cmake/cyberwave_mqtt_client
)