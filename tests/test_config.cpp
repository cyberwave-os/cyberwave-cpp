/**
 * Symmetric with Python config behavior: defaults, env overrides.
 */
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cyberwave/config.h>
#include <string>

using namespace cyberwave;

namespace
{

void set_env_var(const char* key, const char* value)
{
#ifdef _WIN32
    std::string assignment = std::string(key) + "=" + value;
    _putenv(assignment.c_str());
#else
    setenv(key, value, 1);
#endif
}

void unset_env_var(const char* key)
{
#ifdef _WIN32
    std::string assignment = std::string(key) + "=";
    _putenv(assignment.c_str());
#else
    unsetenv(key);
#endif
}

} // namespace

static void test_defaults()
{
    Config c;
    assert(c.base_url == DEFAULT_BASE_URL);
    assert(c.mqtt_host == DEFAULT_MQTT_HOST);
    assert(c.mqtt_port == DEFAULT_MQTT_PORT);
    assert(c.mqtt_username == DEFAULT_MQTT_USERNAME);
    assert(c.timeout_seconds == DEFAULT_TIMEOUT);
    assert(c.source_type == SOURCE_TYPE_EDGE);
    assert(c.runtime_mode == "live");
}

static void test_env_overrides()
{
    set_env_var("CYBERWAVE_BASE_URL", "https://custom.example.com");
    set_env_var("CYBERWAVE_API_KEY", "test-key");
    set_env_var("CYBERWAVE_MQTT_PORT", "1883");
    set_env_var("CYBERWAVE_MQTT_PROTOCOL", "5");
    set_env_var("CYBERWAVE_TWIN_UUID", "twin-123");
    Config c;
    c.load_from_environment();
    assert(c.base_url == "https://custom.example.com");
    assert(c.api_key == "test-key");
    assert(c.mqtt_port == 1883);
    assert(c.mqtt_protocol == 5);
    assert(c.twin_uuid == "twin-123");
    unset_env_var("CYBERWAVE_BASE_URL");
    unset_env_var("CYBERWAVE_API_KEY");
    unset_env_var("CYBERWAVE_MQTT_PORT");
    unset_env_var("CYBERWAVE_MQTT_PROTOCOL");
    unset_env_var("CYBERWAVE_TWIN_UUID");
}

static void test_runtime_mode_simulation_sets_default_source_type()
{
    unset_env_var("CYBERWAVE_SOURCE_TYPE");
    set_env_var("CYBERWAVE_RUNTIME_MODE", "simulation");

    Config c;
    c.load_from_environment();
    assert(c.runtime_mode == "simulation");
    assert(c.source_type == SOURCE_TYPE_SIM);

    unset_env_var("CYBERWAVE_RUNTIME_MODE");
}

static void test_source_type_env_override_is_honored()
{
    set_env_var("CYBERWAVE_RUNTIME_MODE", "simulation");
    set_env_var("CYBERWAVE_SOURCE_TYPE", "tele");

    Config c;
    c.load_from_environment();
    assert(c.runtime_mode == "simulation");
    assert(c.source_type == "tele");

    unset_env_var("CYBERWAVE_RUNTIME_MODE");
    unset_env_var("CYBERWAVE_SOURCE_TYPE");
}

static void test_explicit_edge_source_type_is_not_overridden_by_preconfigured_simulation_mode()
{
    unset_env_var("CYBERWAVE_RUNTIME_MODE");
    unset_env_var("CYBERWAVE_SOURCE_TYPE");

    Config c;
    c.runtime_mode = "simulation";
    c.source_type = SOURCE_TYPE_EDGE;
    c.load_from_environment();

    assert(c.runtime_mode == "simulation");
    assert(c.source_type == SOURCE_TYPE_EDGE);
}

int main()
{
    test_defaults();
    test_env_overrides();
    test_runtime_mode_simulation_sets_default_source_type();
    test_source_type_env_override_is_honored();
    test_explicit_edge_source_type_is_not_overridden_by_preconfigured_simulation_mode();
    return 0;
}
