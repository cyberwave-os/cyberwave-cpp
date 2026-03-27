/**
 * @brief Configuration values and defaults for the Cyberwave C++ SDK.
 */

#ifndef CYBERWAVE_CONFIG_H
#define CYBERWAVE_CONFIG_H

#include "cyberwave/constants.h"

#include <cstdint>
#include <string>

namespace cyberwave
{

/** @brief Production default REST base URL. */
constexpr const char* DEFAULT_BASE_URL = "https://api.cyberwave.com";

/** @brief Production default MQTT host. */
constexpr const char* DEFAULT_MQTT_HOST = "mqtt.cyberwave.com";

/** @brief Production default MQTT port. */
constexpr std::uint16_t DEFAULT_MQTT_PORT = 8883;

/** @brief Production default MQTT username. */
constexpr const char* DEFAULT_MQTT_USERNAME = "mqttcyb";

/** @brief Default request timeout in seconds. */
constexpr int DEFAULT_TIMEOUT = 30;

/**
 * @brief SDK configuration object mirroring Python `CyberwaveConfig`.
 */
struct Config
{
    std::string base_url{DEFAULT_BASE_URL};
    std::string api_key;
    std::string mqtt_host{DEFAULT_MQTT_HOST};
    std::uint16_t mqtt_port{DEFAULT_MQTT_PORT};
    std::string mqtt_username{DEFAULT_MQTT_USERNAME};
    /** Separate MQTT password; when non-empty used instead of api_key for broker auth (e.g. CI/local broker). */
    std::string mqtt_password;
    bool mqtt_use_tls{true}; // default TLS on port 8883, mirrors Python CyberwaveConfig
    std::string mqtt_tls_ca_cert;
    std::string environment_id;
    std::string workspace_id;
    int timeout_seconds{DEFAULT_TIMEOUT};
    bool verify_ssl{true};
    std::string source_type{SOURCE_TYPE_EDGE}; // from constants.h
    std::string topic_prefix;

    /**
     * @brief Load configuration fields from environment variables.
     *
     * Reads values such as `CYBERWAVE_BASE_URL`, `CYBERWAVE_API_KEY`,
     * `CYBERWAVE_MQTT_HOST`, and related MQTT settings.
     */
    void load_from_environment();
};

} // namespace cyberwave

#endif // CYBERWAVE_CONFIG_H
