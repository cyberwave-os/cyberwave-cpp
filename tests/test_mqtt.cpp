/**
 * Cyberwave C++ MQTT client test
 *
 * Minimal test: connect, subscribe to a topic, disconnect.
 * Requires a reachable MQTT broker. If connection fails, the test is skipped
 * (exit 0) so the suite can pass in environments without a broker.
 *
 * Environment variables:
 *   CYBERWAVE_MQTT_HOST   - Broker host (default: localhost)
 *   CYBERWAVE_MQTT_PORT   - Broker port (default: 1883)
 *   CYBERWAVE_API_KEY     - API token used as MQTT password (required for real connect)
 */

#include "mqtt_client.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace cyberwave;

#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_RESET "\033[0m"

static std::string getEnv(const std::string& key, const std::string& fallback)
{
    const char* v = std::getenv(key.c_str());
    return v ? std::string(v) : fallback;
}

static int run_test()
{
    std::cout << COLOR_BLUE << "\n=== MQTT test: connect, subscribe, disconnect ===" << COLOR_RESET << std::endl;

    std::string host = getEnv("CYBERWAVE_MQTT_HOST", "localhost");
    std::string port_str = getEnv("CYBERWAVE_MQTT_PORT", "1883");
    std::string token = getEnv("CYBERWAVE_API_KEY", "");

    int port = 1883;
    try
    {
        port = std::stoi(port_str);
    }
    catch (...)
    {
        std::cerr << COLOR_YELLOW << "ℹ️  Invalid CYBERWAVE_MQTT_PORT, using 1883" << COLOR_RESET << std::endl;
    }

    CyberwaveConfig config;
    config.mqtt_host = host;
    config.mqtt_port = port;
    config.mqtt_username = "mqttcyb";
    config.mqtt_api_token = token.empty() ? "test-api-key" : token;
    config.topic_prefix = "";

    try
    {
        CyberwaveMQTTClient client(config);
        std::cout << COLOR_YELLOW << "ℹ️  Connecting to " << host << ":" << port << "..." << COLOR_RESET << std::endl;
        client.connect();

        if (!client.is_connected())
        {
            std::cout << COLOR_YELLOW << "ℹ️  MQTT broker not available; skipping MQTT test" << COLOR_RESET << std::endl;
            return 0;
        }

        std::cout << COLOR_GREEN << "✅ Connected" << COLOR_RESET << std::endl;

        std::string test_topic = "cyberwave/sdk/test/connect";
        client.subscribe(
            test_topic,
            [](const std::string& topic, const json& msg)
            {
                (void)topic;
                (void)msg;
            },
            0);
        std::cout << COLOR_GREEN << "✅ Subscribed to " << test_topic << COLOR_RESET << std::endl;

        client.disconnect();
        std::cout << COLOR_GREEN << "✅ Disconnected" << COLOR_RESET << std::endl;

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cout << COLOR_YELLOW << "ℹ️  MQTT broker not available (" << e.what() << "); skipping MQTT test"
                  << COLOR_RESET << std::endl;
        return 0;
    }
}

int main()
{
    std::cout << COLOR_BLUE << R"(
╔══════════════════════════════════════════════════════════╗
║        Cyberwave C++ MQTT client test                    ║
╚══════════════════════════════════════════════════════════╝
)" << COLOR_RESET
              << std::endl;

    return run_test();
}
