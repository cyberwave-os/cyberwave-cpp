#include "cyberwave/edge/edge_config.h"
#include "cyberwave/exceptions.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace cyberwave
{

namespace
{

std::string getenv_default(const char* name, const char* default_value)
{
    const char* v = std::getenv(name);
    if (v && v[0] != '\0')
        return v;
    return default_value ? default_value : "";
}

int getenv_int_default(const char* name, int default_value)
{
    const char* v = std::getenv(name);
    if (!v || v[0] == '\0')
        return default_value;
    try
    {
        return std::stoi(v);
    }
    catch (...)
    {
        return default_value;
    }
}

} // namespace

EdgeNodeConfig EdgeNodeConfig::from_env()
{
    EdgeNodeConfig c;
    c.cyberwave_api_key = getenv_default("CYBERWAVE_API_KEY", "");
    c.cyberwave_base_url = getenv_default("CYBERWAVE_BASE_URL", "https://api.cyberwave.com");
    c.mqtt_host = getenv_default("MQTT_HOST", "");
    int port = getenv_int_default("MQTT_PORT", 1883);
    c.mqtt_port = (port > 0 && port <= 65535) ? static_cast<std::uint16_t>(port) : 1883;
    c.mqtt_username = getenv_default("MQTT_USERNAME", "");
    c.topic_prefix = getenv_default("TOPIC_PREFIX", "");
    c.edge_uuid = getenv_default("EDGE_UUID", "");
    c.twin_uuid = getenv_default("TWIN_UUID", "");
    c.health_publish_interval_sec = getenv_int_default("HEALTH_INTERVAL", 5);
    if (c.health_publish_interval_sec < 1)
        c.health_publish_interval_sec = 5;
    c.log_level = getenv_default("LOG_LEVEL", "INFO");
    c.source_type = getenv_default("SOURCE_TYPE", "edge");
    return c;
}

void EdgeNodeConfig::validate() const
{
    if (cyberwave_api_key.empty())
        throw CyberwaveError("CYBERWAVE_API_KEY is required. Get yours at https://cyberwave.com/profile");
    if (edge_uuid.empty())
        throw CyberwaveError("EDGE_UUID is required. Set it via environment variable or config.");
}

Config EdgeNodeConfig::to_sdk_config() const
{
    Config c;
    c.base_url = cyberwave_base_url;
    c.api_key = cyberwave_api_key;
    c.mqtt_host = mqtt_host.empty() ? DEFAULT_MQTT_HOST : mqtt_host;
    c.mqtt_port = mqtt_port;
    c.mqtt_username = mqtt_username.empty() ? DEFAULT_MQTT_USERNAME : mqtt_username;
    c.topic_prefix = topic_prefix;
    c.source_type = source_type;
    return c;
}

} // namespace cyberwave
