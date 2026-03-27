/**
 * @brief Configuration object for Cyberwave edge nodes.
 */

#ifndef CYBERWAVE_EDGE_EDGE_CONFIG_H
#define CYBERWAVE_EDGE_EDGE_CONFIG_H

#include "cyberwave/config.h"

#include <cstdint>
#include <string>

namespace cyberwave
{

/**
 * @brief Edge node configuration mirroring Python `EdgeNodeConfig`.
 */
struct EdgeNodeConfig
{
    // Cyberwave connection
    std::string cyberwave_api_key;
    std::string cyberwave_base_url{"https://api.cyberwave.com"};

    // MQTT
    std::string mqtt_host;
    std::uint16_t mqtt_port{1883};
    std::string mqtt_username;

    // Topic prefix (environment-specific)
    std::string topic_prefix;

    // Device identity
    std::string edge_uuid;
    std::string twin_uuid; // optional default twin

    // Health & resilience
    int health_publish_interval_sec{5};
    std::string log_level{"INFO"};
    std::string source_type{"edge"};

    /** Create config from environment variables. */
    static EdgeNodeConfig from_env();

    /** Validate required fields; throws CyberwaveError if invalid. */
    void validate() const;

    /** Build SDK Config for Client (base_url, api_key, mqtt_*, topic_prefix, source_type). */
    Config to_sdk_config() const;
};

} // namespace cyberwave

#endif // CYBERWAVE_EDGE_EDGE_CONFIG_H
