/**
 * Symmetric with Python config behavior: defaults, env overrides.
 */
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cyberwave/config.h>
#include <string>

using namespace cyberwave;

static void test_defaults()
{
    Config c;
    assert(c.base_url == DEFAULT_BASE_URL);
    assert(c.mqtt_host == DEFAULT_MQTT_HOST);
    assert(c.mqtt_port == DEFAULT_MQTT_PORT);
    assert(c.mqtt_username == DEFAULT_MQTT_USERNAME);
    assert(c.timeout_seconds == DEFAULT_TIMEOUT);
    assert(c.source_type == SOURCE_TYPE_EDGE);
}

static void test_env_overrides()
{
#ifdef _WIN32
    _putenv("CYBERWAVE_BASE_URL=https://custom.example.com");
    _putenv("CYBERWAVE_API_KEY=test-key");
    _putenv("CYBERWAVE_MQTT_PORT=1883");
#else
    setenv("CYBERWAVE_BASE_URL", "https://custom.example.com", 1);
    setenv("CYBERWAVE_API_KEY", "test-key", 1);
    setenv("CYBERWAVE_MQTT_PORT", "1883", 1);
#endif
    Config c;
    c.load_from_environment();
    assert(c.base_url == "https://custom.example.com");
    assert(c.api_key == "test-key");
    assert(c.mqtt_port == 1883);
#ifdef _WIN32
    _putenv("CYBERWAVE_BASE_URL=");
    _putenv("CYBERWAVE_API_KEY=");
    _putenv("CYBERWAVE_MQTT_PORT=");
#else
    unsetenv("CYBERWAVE_BASE_URL");
    unsetenv("CYBERWAVE_API_KEY");
    unsetenv("CYBERWAVE_MQTT_PORT");
#endif
}

int main()
{
    test_defaults();
    test_env_overrides();
    return 0;
}
