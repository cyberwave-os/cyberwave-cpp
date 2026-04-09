cmake_minimum_required(VERSION 3.10)
project(CyberwaveSDKTests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)

# Find the CppRestOpenAPIClient library
# It should be installed in /usr/local by the build process
find_library(CPPREST_OPENAPI_CLIENT_LIB
    NAMES CppRestOpenAPIClient libCppRestOpenAPIClient
    PATHS /usr/local/lib
    REQUIRED
)

# Include directories
include_directories(
    /usr/local/include
    ${OPENSSL_INCLUDE_DIR}
)

# Create test executable
add_executable(test_sdk test_sdk.cpp)

# Link libraries
target_link_libraries(test_sdk
    ${CPPREST_OPENAPI_CLIENT_LIB}
    cpprestsdk::cpprest
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Optional MQTT test: connect, subscribe, disconnect
set(MQTT_TEST_BUILT OFF)
if(EXISTS ${CMAKE_SOURCE_DIR}/../mqtt/mqtt_client.cpp)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND AND NOT TARGET PkgConfig::MOSQUITTO)
        pkg_check_modules(MOSQUITTO QUIET IMPORTED_TARGET libmosquitto)
    endif()
    find_package(nlohmann_json QUIET)
    find_package(spdlog QUIET)
    if(TARGET PkgConfig::MOSQUITTO AND nlohmann_json_FOUND AND spdlog_FOUND)
        add_library(cyberwave_mqtt_client STATIC ${CMAKE_SOURCE_DIR}/../mqtt/mqtt_client.cpp)
        target_include_directories(cyberwave_mqtt_client
            PUBLIC ${CMAKE_SOURCE_DIR}/../mqtt /usr/local/include)
        target_link_libraries(cyberwave_mqtt_client
            PUBLIC PkgConfig::MOSQUITTO nlohmann_json::nlohmann_json spdlog::spdlog)
        set_target_properties(cyberwave_mqtt_client PROPERTIES CXX_STANDARD 20)

        add_executable(test_mqtt test_mqtt.cpp)
        target_include_directories(test_mqtt PRIVATE ${CMAKE_SOURCE_DIR}/../mqtt /usr/local/include)
        target_link_libraries(test_mqtt PRIVATE cyberwave_mqtt_client)
        set_target_properties(test_mqtt PROPERTIES CXX_STANDARD 17)
        set(MQTT_TEST_BUILT ON)
        message(STATUS "MQTT test enabled (test_mqtt)")
    else()
        message(STATUS "MQTT test skipped: libmosquitto, nlohmann_json, or spdlog not found")
    endif()
else()
    message(STATUS "MQTT test skipped: mqtt sources not found")
endif()

# Enable testing
enable_testing()
add_test(NAME SDKTests COMMAND test_sdk)
if(MQTT_TEST_BUILT)
    add_test(NAME MQTTConnectSubscribeDisconnect COMMAND test_mqtt)
endif()

# Print configuration for debugging
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "CppRest OpenAPI Client Library: ${CPPREST_OPENAPI_CLIENT_LIB}")
message(STATUS "OpenSSL Libraries: ${OPENSSL_LIBRARIES}")

