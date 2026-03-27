/*
  mqtt_example.cpp

  Example usage of the Cyberwave C++ MQTT client.

  This program shows a minimal flow for connecting to an MQTT broker,
  subscribing to twin-related topics (position, rotation, joint states),
  publishing example updates (position, rotation, joint state) and sending
  a ping. Incoming messages are printed to stdout.

  Key actions demonstrated:
   - Configure the MQTT client using CyberwaveConfig
   - Connect the CyberwaveMQTTClient to a broker
   - Subscribe to twin position/rotation and joint state topics
   - Publish position/rotation/joint updates and a ping
   - Keep the client running to receive and log messages

  Before running: adjust `config` (host/port/credentials) and `twin_uuid`
  to match your environment.
*/

#include "mqtt_client.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using namespace cyberwave;

int main()
{
    const char* token_env = std::getenv("CYBERWAVE_API_KEY");
    const std::string mqtt_api_token = token_env ? token_env : "";
    if (mqtt_api_token.empty())
    {
        std::cerr << "CYBERWAVE_API_KEY is required for MQTT authentication" << std::endl;
        return 2;
    }

    // Configure client
    CyberwaveConfig config;

    config.mqtt_host = "mqtt.cyberwave.com";
    config.mqtt_port = 1883;
    config.mqtt_username = "mqttcyb";
    config.mqtt_api_token = mqtt_api_token;

    try
    {
        // Create and connect client
        CyberwaveMQTTClient client(config);
        std::cout << "Connecting to MQTT broker...\n";
        client.connect();

        std::string twin_uuid = "example-twin-uuid-123";

        // Subscribe to position updates
        client.subscribe_twin_position(twin_uuid,
                                       [](const std::string& topic, const json& message)
                                       {
                                           std::cout << "Position update received on topic: " << topic << std::endl;
                                           std::cout << "Position: x=" << message["x"] << ", y=" << message["y"]
                                                     << ", z=" << message["z"] << std::endl;
                                       });

        // Subscribe to rotation updates
        client.subscribe_twin_rotation(twin_uuid,
                                       [](const std::string& topic, const json& message)
                                       {
                                           std::cout << "Rotation update received on topic: " << topic << std::endl;
                                           std::cout << "Rotation: x=" << message["x"] << ", y=" << message["y"]
                                                     << ", z=" << message["z"] << ", w=" << message["w"] << std::endl;
                                       });

        // Subscribe to joint states
        client.subscribe_joint_states(twin_uuid,
                                      [](const std::string& topic, const json& message)
                                      {
                                          std::cout << "Joint state update received on topic: " << topic << std::endl;
                                          std::cout << "Message: " << message.dump(2) << std::endl;
                                      });

        // Wait a bit for subscriptions to be established
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Publish some test data
        std::cout << "\nPublishing test data...\n";

        // Update position
        Position pos{1.0, 2.0, 3.0};
        client.update_twin_position(twin_uuid, pos);
        std::cout << "Published position update\n";

        // Update rotation
        Rotation rot{0.0, 0.0, 0.707, 0.707};
        client.update_twin_rotation(twin_uuid, rot);
        std::cout << "Published rotation update\n";

        // Update joint state
        JointState joint;
        joint.position = 1.57;
        joint.velocity = 0.1;
        joint.effort = 10.5;
        client.update_joint_state(twin_uuid, "joint1", joint);
        std::cout << "Published joint state update\n";

        // Send ping
        client.ping(twin_uuid);
        std::cout << "Sent ping\n";

        // Keep running to receive messages
        std::cout << "\nListening for messages (press Ctrl+C to exit)...\n";
        while (client.is_connected())
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}