/**
 * Symmetric with Python test_mqtt_auth.py:
 * MQTT config defaults and environment-variable overrides.
 *
 * Python test_mqtt_auth.py tests actual MQTT client credential selection;
 * in C++ we test at the Config level since IMqttClient is an interface.
 */

#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>

#include <cassert>
#include <cstdlib>
#include <string>

using namespace cyberwave;

/** Mirrors test_config_defaults_to_tls_mqtt_port */
static void test_config_default_mqtt_port_is_tls()
{
    Config cfg;
    assert(cfg.mqtt_port == 8883);
    assert(cfg.mqtt_use_tls == true);
}

/** Default MQTT host is set */
static void test_config_default_mqtt_host()
{
    Config cfg;
    assert(!cfg.mqtt_host.empty());
    assert(cfg.mqtt_host == "mqtt.cyberwave.com");
}

/** Default MQTT username is set */
static void test_config_default_mqtt_username()
{
    Config cfg;
    assert(!cfg.mqtt_username.empty());
}

/** Mirrors test_base_client_uses_api_key_when_no_mqtt_password:
 *  Config.api_key is used as MQTT password when no separate mqtt_password provided.
 *  In C++ we just verify the api_key field is settable and readable. */
static void test_config_api_key_settable()
{
    Config cfg;
    cfg.api_key = "api_key_secret";
    assert(cfg.api_key == "api_key_secret");
}

/** Mirrors test_config_env_override_mqtt_host:
 *  CYBERWAVE_MQTT_HOST env var overrides default. */
static void test_config_env_override_mqtt_host()
{
    ::setenv("CYBERWAVE_MQTT_HOST", "mqtt.test.local", 1);
    Config cfg;
    cfg.load_from_environment();
    assert(cfg.mqtt_host == "mqtt.test.local");
    ::unsetenv("CYBERWAVE_MQTT_HOST");
}

/** Mirrors test_config_env_override_mqtt_port */
static void test_config_env_override_mqtt_port()
{
    ::setenv("CYBERWAVE_MQTT_PORT", "1883", 1);
    Config cfg;
    cfg.load_from_environment();
    assert(cfg.mqtt_port == 1883);
    ::unsetenv("CYBERWAVE_MQTT_PORT");
}

/** Mirrors test_config_env_override_mqtt_use_tls */
static void test_config_env_override_mqtt_use_tls()
{
    ::setenv("CYBERWAVE_MQTT_USE_TLS", "false", 1);
    Config cfg;
    cfg.load_from_environment();
    assert(cfg.mqtt_use_tls == false);
    ::unsetenv("CYBERWAVE_MQTT_USE_TLS");
}

/** CYBERWAVE_API_KEY env var overrides api_key */
static void test_config_env_override_api_key()
{
    ::setenv("CYBERWAVE_API_KEY", "env-api-key", 1);
    Config cfg;
    cfg.load_from_environment();
    assert(cfg.api_key == "env-api-key");
    ::unsetenv("CYBERWAVE_API_KEY");
}

/** Config env override for base_url */
static void test_config_env_override_base_url()
{
    ::setenv("CYBERWAVE_BASE_URL", "https://my.backend.test", 1);
    Config cfg;
    cfg.load_from_environment();
    assert(cfg.base_url == "https://my.backend.test");
    ::unsetenv("CYBERWAVE_BASE_URL");
}

static void test_config_env_override_mqtt_protocol_and_twin_uuid()
{
    ::setenv("CYBERWAVE_MQTT_PROTOCOL", "mqttv5", 1);
    ::setenv("CYBERWAVE_TWIN_UUID", "edge-twin", 1);
    Config cfg;
    cfg.load_from_environment();
    assert(cfg.mqtt_protocol == 5);
    assert(cfg.twin_uuid == "edge-twin");
    ::unsetenv("CYBERWAVE_MQTT_PROTOCOL");
    ::unsetenv("CYBERWAVE_TWIN_UUID");
}

int main()
{
    test_config_default_mqtt_port_is_tls();
    test_config_default_mqtt_host();
    test_config_default_mqtt_username();
    test_config_api_key_settable();
    test_config_env_override_mqtt_host();
    test_config_env_override_mqtt_port();
    test_config_env_override_mqtt_use_tls();
    test_config_env_override_api_key();
    test_config_env_override_base_url();
    test_config_env_override_mqtt_protocol_and_twin_uuid();
    return 0;
}
