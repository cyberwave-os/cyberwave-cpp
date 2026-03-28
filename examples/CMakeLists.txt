cmake_minimum_required(VERSION 3.10)
project(cyberwave_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Place all built executables/libraries for this project in the examples build dir
# This ensures rest_example and mqtt_example end up in the same folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

## REST examples depend on the generated REST SDK headers (CppRestOpenAPIClient).
## If not present, skip building these executables (MQTT-only examples still build).
find_path(CPPREST_OPENAPI_INCLUDE_DIR
    NAMES CppRestOpenAPIClient/ApiClient.h
    PATHS
        ${CMAKE_SOURCE_DIR}/../rest/include
        /usr/local/include
        /usr/include
)

if (CPPREST_OPENAPI_INCLUDE_DIR)
    message(STATUS "Found REST SDK headers: ${CPPREST_OPENAPI_INCLUDE_DIR}")

    # Required deps for REST example
    find_package(cpprestsdk REQUIRED)
    find_package(OpenSSL REQUIRED)

    ## Build rest_example in its own out-of-tree build directory (rest_build)
    add_executable(rest_example rest_example.cpp)

    # Always link cpprest
    target_link_libraries(rest_example PRIVATE cpprestsdk::cpprest)

    # Use local SDK headers (project-relative) and common system include
    target_include_directories(rest_example PRIVATE ${CMAKE_SOURCE_DIR}/../include ${CPPREST_OPENAPI_INCLUDE_DIR} /usr/local/include)

    # Optionally link generated REST SDK library if installed
    find_library(CPPREST_OPENAPI_CLIENT NAMES CppRestOpenAPIClient libCppRestOpenAPIClient PATHS /usr/local/lib /usr/lib)
    if (CPPREST_OPENAPI_CLIENT)
        message(STATUS "Found generated REST SDK library: ${CPPREST_OPENAPI_CLIENT}")
        target_link_libraries(rest_example PRIVATE ${CPPREST_OPENAPI_CLIENT} OpenSSL::SSL OpenSSL::Crypto)
    else()
        message(WARNING "Generated REST SDK library (CppRestOpenAPIClient) not found. REST examples may fail to link.")
    endif()

    # Ensure rest binary ends up in build/rest_build
    set_target_properties(rest_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/rest_build
    )
    install(TARGETS rest_example RUNTIME DESTINATION bin)
else()
    message(WARNING "REST SDK headers (CppRestOpenAPIClient) not found; skipping REST examples (rest_example, mixed_example).")
endif()

## Build a MQTT example and its static library from the sibling mqtt sources
if (EXISTS ${CMAKE_SOURCE_DIR}/../mqtt/mqtt_client.cpp)
    message(STATUS "Found mqtt sources; building cyberwave_mqtt_client and mqtt_example in-tree")

    # Find required packages for mqtt implementation
    find_package(PahoMqttCpp REQUIRED)
    find_package(nlohmann_json REQUIRED)
    find_package(spdlog REQUIRED)

    # Build static library from the mqtt source file (compile inside this build tree)
    add_library(cyberwave_mqtt_client STATIC ${CMAKE_SOURCE_DIR}/../mqtt/mqtt_client.cpp)
    target_include_directories(cyberwave_mqtt_client
        PUBLIC
            ${CMAKE_SOURCE_DIR}/../mqtt
            ${CMAKE_SOURCE_DIR}/../include
            /usr/local/include
    )
    target_link_libraries(cyberwave_mqtt_client
        PUBLIC PahoMqttCpp::paho-mqttpp3
        PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog
    )

    # Place the static archive and the mqtt_example runtime binary in build/mqtt_build
    set_target_properties(cyberwave_mqtt_client PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mqtt_build
    )

    add_executable(mqtt_example mqtt_example.cpp)
    target_include_directories(mqtt_example PRIVATE ${CMAKE_SOURCE_DIR}/../include /usr/local/include)
    target_link_libraries(mqtt_example PRIVATE cyberwave_mqtt_client)
    set_target_properties(mqtt_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mqtt_build
    )
    install(TARGETS mqtt_example RUNTIME DESTINATION bin)
    
    add_executable(twin_example twin_example.cpp)
    target_include_directories(twin_example PRIVATE ${CMAKE_SOURCE_DIR}/../include /usr/local/include)
    target_link_libraries(twin_example PRIVATE cyberwave_mqtt_client)
    set_target_properties(twin_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mqtt_build
    )
    install(TARGETS twin_example RUNTIME DESTINATION bin)

    add_executable(simple_update_example simple_update_example.cpp)
    target_include_directories(simple_update_example PRIVATE ${CMAKE_SOURCE_DIR}/../include /usr/local/include)
    target_link_libraries(simple_update_example PRIVATE cyberwave_mqtt_client)
    set_target_properties(simple_update_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mqtt_build
    )
    install(TARGETS simple_update_example RUNTIME DESTINATION bin)

    add_executable(complex_update_example complex_update_example.cpp)
    target_include_directories(complex_update_example PRIVATE ${CMAKE_SOURCE_DIR}/../include /usr/local/include)
    target_link_libraries(complex_update_example PRIVATE cyberwave_mqtt_client)
    set_target_properties(complex_update_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mqtt_build
    )
    install(TARGETS complex_update_example RUNTIME DESTINATION bin)

    # mixed_example uses both REST and MQTT: list twins via REST then actuate via MQTT
    if (CPPREST_OPENAPI_INCLUDE_DIR)
        add_executable(mixed_example mixed_example.cpp)
        target_include_directories(mixed_example PRIVATE ${CMAKE_SOURCE_DIR}/../include ${CPPREST_OPENAPI_INCLUDE_DIR} /usr/local/include)
        target_link_libraries(mixed_example PRIVATE cyberwave_mqtt_client cpprestsdk::cpprest)
        if (CPPREST_OPENAPI_CLIENT)
            target_link_libraries(mixed_example PRIVATE ${CPPREST_OPENAPI_CLIENT} OpenSSL::SSL OpenSSL::Crypto)
        endif()
        set_target_properties(mixed_example PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mqtt_build
        )
        install(TARGETS mixed_example RUNTIME DESTINATION bin)
    endif()

else()
    message(STATUS "mqtt sources not found next to examples/; mqtt_example will not be built")
endif()
