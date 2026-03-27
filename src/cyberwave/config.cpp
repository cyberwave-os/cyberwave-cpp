#include "cyberwave/config.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace cyberwave
{

static std::string getenv_default(const char* name, const char* default_value)
{
    const char* v = std::getenv(name);
    if (v && v[0] != '\0')
        return v;
    return default_value ? default_value : "";
}

static int getenv_int_default(const char* name, int default_value)
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

static bool getenv_bool(const char* name, bool default_value)
{
    const char* v = std::getenv(name);
    if (!v)
        return default_value;
    std::string s = v;
    while (!s.empty() && s.back() == ' ')
        s.pop_back();
    size_t i = 0;
    while (i < s.size() && s[i] == ' ')
        ++i;
    s = s.substr(i);
    if (s.empty())
        return default_value;
    if (s == "1" || s == "true" || s == "yes" || s == "on")
        return true;
    if (s == "0" || s == "false" || s == "no" || s == "off")
        return false;
    return default_value;
}

void Config::load_from_environment()
{
    {
        std::string env_key = getenv_default("CYBERWAVE_API_KEY", "");
        if (!env_key.empty())
            api_key = env_key;
    }
    if (base_url == DEFAULT_BASE_URL)
    {
        base_url = getenv_default("CYBERWAVE_BASE_URL", DEFAULT_BASE_URL);
    }
    {
        // Override if still at default or explicitly empty
        std::string env_host = getenv_default("CYBERWAVE_MQTT_HOST", "");
        if (!env_host.empty() && (mqtt_host.empty() || mqtt_host == DEFAULT_MQTT_HOST))
            mqtt_host = env_host;
        else if (mqtt_host.empty())
            mqtt_host = DEFAULT_MQTT_HOST;
    }
    {
        int env_port = getenv_int_default("CYBERWAVE_MQTT_PORT", -1);
        if (env_port >= 0)
            mqtt_port = static_cast<std::uint16_t>(env_port);
        else if (mqtt_port == 0)
            mqtt_port = DEFAULT_MQTT_PORT;
    }
    {
        std::string env_u = getenv_default("CYBERWAVE_MQTT_USERNAME", "");
        if (!env_u.empty())
            mqtt_username = env_u;
        else if (mqtt_username.empty())
            mqtt_username = DEFAULT_MQTT_USERNAME;
    }
    if (mqtt_password.empty())
    {
        mqtt_password = getenv_default("CYBERWAVE_MQTT_PASSWORD", "");
    }
    mqtt_use_tls = getenv_bool("CYBERWAVE_MQTT_USE_TLS", mqtt_use_tls);
    if (mqtt_port == 8883 && !mqtt_use_tls)
        mqtt_use_tls = true;
    if (mqtt_tls_ca_cert.empty())
    {
        mqtt_tls_ca_cert = getenv_default("CYBERWAVE_MQTT_TLS_CA_CERT", "");
    }
    if (environment_id.empty())
    {
        environment_id = getenv_default("CYBERWAVE_ENVIRONMENT_ID", "");
    }
    if (workspace_id.empty())
    {
        workspace_id = getenv_default("CYBERWAVE_WORKSPACE_ID", "");
    }
    if (source_type.empty())
    {
        source_type = getenv_default("CYBERWAVE_SOURCE_TYPE", SOURCE_TYPE_EDGE);
    }
    if (topic_prefix.empty())
    {
        topic_prefix = getenv_default("CYBERWAVE_MQTT_TOPIC_PREFIX", "");
        if (topic_prefix.empty())
        {
            std::string env_val = getenv_default("CYBERWAVE_ENVIRONMENT", "");
            if (!env_val.empty() && env_val != "production")
                topic_prefix = env_val;
        }
    }
    if (timeout_seconds == DEFAULT_TIMEOUT)
    {
        int t = getenv_int_default("CYBERWAVE_TIMEOUT", DEFAULT_TIMEOUT);
        if (t > 0)
            timeout_seconds = t;
    }
}

} // namespace cyberwave
