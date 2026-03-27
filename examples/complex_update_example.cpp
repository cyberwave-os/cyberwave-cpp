/*
    complex_update_example.cpp

    Publish joint updates for multiple joints (all joints) to a twin that
    resides in an environment. Demonstrates the batch helper
    `update_joint_states(...)` which sends multiple joint-state messages.

    Key actions demonstrated:
     - Build a map of joint name -> JointState for multiple joints
     - Reuse the MQTT client's batch helper to publish all joint states
     - Subscribe to twin topics to observe published messages

    Usage:
        ./complex_update_example <twin_uuid>
        or set EXAMPLE_TWIN_UUID environment variable
*/

#include "mqtt_client.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
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

// Reuse oscillation helper from other examples: advance position in -PI..PI
static void oscillate_joint_position(JointState& joint)
{
    const double PI = 3.14159265358979323846;
    static const double delta = 0.05; // smaller step so multi-joint updates are smoother
    static int dir = 1;               // +1 = increasing, -1 = decreasing

    if (!joint.position.has_value())
    {
        joint.position = -PI;
        dir = 1;
    }
    else
    {
        double v = joint.position.value() + dir * delta;
        if (v >= PI)
        {
            v = PI;
            dir = -1;
        }
        else if (v <= -PI)
        {
            v = -PI;
            dir = 1;
        }
        joint.position = v;
    }
}

int main(int argc, char** argv)
{
    std::string twin_uuid = (argc >= 2) ? argv[1] : getenv_or("EXAMPLE_TWIN_UUID", "example-twin-uuid-123");
    std::string env_uuid = getenv_or("EXAMPLE_ENV_UUID", getenv_or("ENVIRONMENT", ""));
    const std::string mqtt_api_token = getenv_or("CYBERWAVE_API_KEY");
    if (mqtt_api_token.empty())
    {
        std::cerr << "CYBERWAVE_API_KEY is required for MQTT authentication" << std::endl;
        return 2;
    }

    CyberwaveConfig config{.mqtt_host = getenv_or("CYBERWAVE_MQTT_HOST", "mqtt.cyberwave.com"),
                           .mqtt_port = 1883,
                           .mqtt_username = getenv_or("CYBERWAVE_MQTT_USERNAME", "mqttcyb"),
                           .mqtt_api_token = mqtt_api_token};

    std::signal(SIGINT, handle_sigint);

    try
    {
        CyberwaveMQTTClient client(config);
        client.connect();

        auto print_cb = [](const std::string& topic, const json& message)
        { std::cout << "[RECV] " << topic << " -> " << message.dump() << std::endl; };

        // Subscribe so we can observe the joint updates we publish
        client.subscribe_twin_position(twin_uuid, print_cb);
        client.subscribe_twin_rotation(twin_uuid, print_cb);
        client.subscribe_joint_states(twin_uuid, print_cb);
        if (!env_uuid.empty())
            client.subscribe_environment(env_uuid, print_cb);

        // Small delay to ensure subscriptions are registered
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Prepare a map of joint states (example: 6 joints)
        std::map<std::string, JointState> joints;
        for (int i = 1; i <= 6; ++i)
        {
            joints[std::to_string(i)] = JointState{};
            joints[std::to_string(i)].velocity = 0.0;
            joints[std::to_string(i)].effort = 0.0;
        }

        // Initial ping for visibility
        client.ping(twin_uuid);
        std::cout << "Sent ping for " << twin_uuid << std::endl;

        std::cout << "Publishing multi-joint updates (Ctrl+C to exit)..." << std::endl;
        while (g_running && client.is_connected())
        {
            // Update each joint position slightly and publish all states in one helper call
            for (auto& kv : joints)
            {
                oscillate_joint_position(kv.second);
                // you could also vary velocity/effort here if desired
            }

            // Batch-publish all joint states
            client.update_joint_states(twin_uuid, joints);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << "Shutting down..." << std::endl;
        client.disconnect();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
