#include "cyberwave/edge/amr_types.h"

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

double getenv_double_default(const char* name, double default_value)
{
    const char* v = std::getenv(name);
    if (!v || v[0] == '\0')
        return default_value;
    try
    {
        return std::stod(v);
    }
    catch (...)
    {
        return default_value;
    }
}

} // namespace

AdapterConfig AdapterConfig::from_env()
{
    AdapterConfig c;
    c.adapter_type = getenv_default("ADAPTER_TYPE", "");
    c.host = getenv_default("ADAPTER_HOST", "");
    int port = getenv_int_default("ADAPTER_PORT", 0);
    c.port = (port > 0 && port <= 65535) ? static_cast<std::uint16_t>(port) : 0;
    c.username = getenv_default("ADAPTER_USERNAME", "");
    c.password = getenv_default("ADAPTER_PASSWORD", "");
    c.api_key = getenv_default("ADAPTER_API_KEY", "");
    c.robot_id = getenv_default("ADAPTER_ROBOT_ID", "");
    c.position_poll_rate_hz = getenv_double_default("POSITION_POLL_RATE_HZ", 10.0);
    if (c.position_poll_rate_hz <= 0.0)
        c.position_poll_rate_hz = 10.0;
    c.telemetry_poll_rate_hz = getenv_double_default("TELEMETRY_POLL_RATE_HZ", 1.0);
    if (c.telemetry_poll_rate_hz <= 0.0)
        c.telemetry_poll_rate_hz = 1.0;
    c.vda_manufacturer = getenv_default("VDA_MANUFACTURER", "");
    c.vda_serial_number = getenv_default("VDA_SERIAL_NUMBER", "");
    // ADAPTER_EXTRA JSON parsing omitted for simplicity; subclasses can extend
    return c;
}

} // namespace cyberwave
