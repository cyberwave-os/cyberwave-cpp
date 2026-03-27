/*
    simple_update_example.cpp

    Minimal MQTT example that connects to a broker, publishes a single joint
    state update and listens for messages on twin-related topics. Intended
    for quick verification of publish/subscribe behavior.

    Key actions demonstrated:
     - Configure the MQTT client using CyberwaveConfig or environment vars
     - Connect the CyberwaveMQTTClient
     - Publish a single joint state update and send a ping
     - Subscribe to twin topics and print inbound JSON messages

    Usage:
        ./simple_update_example <twin_uuid>
        or set EXAMPLE_TWIN_UUID environment variable
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

// Helper: advance joint.position in a bounded oscillation between -PI and PI.
// Maintains internal direction/state across calls so repeated calls will
// smoothly increment/decrement the value.
static void oscillate_joint_position(JointState& joint)
{
    const double PI = 3.14159265358979323846;
    static const double delta = 0.1;
    static int dir = 1; // +1 = increasing, -1 = decreasing

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
    const std::string mqtt_api_token = getenv_or("CYBERWAVE_API_KEY");
    if (mqtt_api_token.empty())
    {
        std::cerr << "CYBERWAVE_API_KEY is required for MQTT authentication" << std::endl;
        return 2;
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

        // Callback that prints any received topic + json
        auto print_cb = [](const std::string& topic, const json& message)
        { std::cout << "[RECV] " << topic << " -> " << message.dump() << std::endl; };

        // Subscribe to twin topics so we can see the messages we publish
        client.subscribe_twin_position(twin_uuid, print_cb);
        client.subscribe_twin_rotation(twin_uuid, print_cb);
        client.subscribe_joint_states(twin_uuid, print_cb);
        client.subscribe_pong(twin_uuid, print_cb);

        // Small pause to ensure subscriptions are set up on the broker
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Publish a joint state update
        JointState joint;
        joint.position = -1.57;
        joint.velocity = 0.05;
        joint.effort = 0.0;

        // Send a ping to show liveness
        client.ping(twin_uuid);
        std::cout << "Sent ping for " << twin_uuid << std::endl;

        // Give broker time to route messages and invoke callbacks
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        std::cout << "Listening for any further messages (Ctrl+C to exit)..." << std::endl;
        while (g_running && client.is_connected())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            oscillate_joint_position(joint);
            client.update_joint_state(twin_uuid, "1", joint);
            std::cout << "Published joint state for " << twin_uuid << "/joint1" << std::endl;
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
