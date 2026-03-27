/*
    twin_example.cpp

    Minimal MQTT client example that subscribes to environment- and
    twin-scoped topics and prints incoming messages to stdout.

    Key actions demonstrated:
     - Read MQTT connection settings from environment variables (optional)
     - Subscribe to environment and twin topics (position, rotation, joints)
     - Print any JSON payloads received for inspection

    Usage:
        ./twin_example <environment_uuid> <twin_uuid>
        or set EXAMPLE_ENV_UUID and/or EXAMPLE_TWIN_UUID environment variables
*/

#include "mqtt_client.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using namespace cyberwave;

static volatile std::sig_atomic_t g_running = 1;

void handle_sigint(int) { g_running = 0; }

static std::string getenv_or(const char* name, const std::string& def = "")
{
    const char* v = std::getenv(name);
    return v ? std::string(v) : def;
}

int main(int argc, char** argv)
{
    auto env_uuid = (argc >= 3) ? argv[1] : getenv_or("EXAMPLE_ENV_UUID");
    auto twin_uuid = (argc >= 3) ? argv[2] : getenv_or("EXAMPLE_TWIN_UUID");
    const std::string mqtt_api_token = getenv_or("CYBERWAVE_API_KEY");
    if (mqtt_api_token.empty())
    {
        std::cerr << "CYBERWAVE_API_KEY is required for MQTT authentication" << std::endl;
        return 2;
    }

    if (env_uuid.empty() && twin_uuid.empty())
    {
        std::cerr << "Usage: " << argv[0] << " <environment_uuid> <twin_uuid>\n";
        std::cerr << "Or set EXAMPLE_ENV_UUID and/or EXAMPLE_TWIN_UUID environment variables.\n";
        return 1;
    }

    // Build config from environment (optional overrides)
    CyberwaveConfig config{.mqtt_host = getenv_or("CYBERWAVE_MQTT_HOST", "mqtt.cyberwave.com"),
                           .mqtt_port = 1883,
                           .mqtt_username = getenv_or("CYBERWAVE_MQTT_USERNAME", "mqttcyb"),
                           .mqtt_api_token = mqtt_api_token};

    // Register Ctrl-C handler
    std::signal(SIGINT, handle_sigint);

    try
    {
        CyberwaveMQTTClient client(config);
        client.connect();

        // Subscribe to environment updates (if provided)
        if (!env_uuid.empty())
        {
            client.subscribe_environment(env_uuid, [](const std::string& topic, const json& message)
                                         { std::cout << "[ENV] " << topic << " -> " << message.dump(2) << std::endl; });
        }

        // Subscribe to twin position and rotation
        if (!twin_uuid.empty())
        {
            client.subscribe_twin_position(
                twin_uuid, [](const std::string& topic, const json& message)
                { std::cout << "[TWIN POSITION] " << topic << " -> " << message.dump(2) << std::endl; });

            client.subscribe_twin_rotation(
                twin_uuid, [](const std::string& topic, const json& message)
                { std::cout << "[TWIN ROTATION] " << topic << " -> " << message.dump(2) << std::endl; });

            // Subscribe to joint updates (wildcard)
            client.subscribe_joint_states(
                twin_uuid, [](const std::string& topic, const json& message)
                { std::cout << "[JOINT] " << topic << " -> " << message.dump(2) << std::endl; });
        }

        std::cout << "MQTT subscriptions registered. Listening... (Ctrl+C to exit)" << std::endl;

        // Keep running until SIGINT
        while (g_running && client.is_connected())
        {
            // 5Hz loop
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        std::cout << "Shutting down...\n";
        client.disconnect();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
