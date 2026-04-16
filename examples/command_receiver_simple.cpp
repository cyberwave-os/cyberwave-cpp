/*
    command_receiver_simple.cpp — Simple command receiver example

    Mirrors: examples/command_receiver_simple.py

    Demonstrates:
     - Wiring the MQTT client into the SDK via CyberwaveMqttAdapter
     - Subscribing to command messages for a twin via MQTT
     - Responding with publish_command_message()
     - Running a blocking event loop (Ctrl+C to stop)

    Quick start:
        1. Set environment variables:
               export CYBERWAVE_API_KEY="your-api-key"
               export TWIN_UUID="your-twin-uuid"
               export CYBERWAVE_MQTT_HOST="mqtt.cyberwave.com"
               # Optional overrides:
               export CYBERWAVE_MQTT_PORT=8883
               export CYBERWAVE_MQTT_USERNAME=mqttcyb

        2. Run the receiver:
               ./example_command_receiver_simple

        3. Send a command to the twin's command topic:
               Payload: {"command": "greetings"}

    Requires: cyberwave_sdk + cyberwave_mqtt_client (libmosquitto)
*/

#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/cyberwave_mqtt_adapter.h>
#include <cyberwave/exceptions.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

static volatile bool g_running = true;

int main()
{
    std::signal(SIGINT, [](int) { g_running = false; });

    const char* twin_uuid_env = std::getenv("TWIN_UUID");
    if (!twin_uuid_env)
    {
        std::cerr << "Please set TWIN_UUID environment variable\n";
        return 1;
    }
    std::string twin_uuid(twin_uuid_env);

    try
    {
        cyberwave::Config cfg;
        cfg.load_from_environment();
        cyberwave::Client client(cfg);

        // Create and connect the MQTT client via the adapter.
        // Mirrors Python: client = Cyberwave(..., mqtt_host=...) which auto-creates mqtt.
        auto adapter = std::make_shared<cyberwave::CyberwaveMqttAdapter>(cfg);
        adapter->connect();
        client.set_mqtt_client(adapter);

        // Subscribe to command messages for this twin
        adapter->subscribe_command_message(
            twin_uuid,
            [&](const std::string& payload)
            {
                // Skip status echo messages
                if (payload.find("\"status\"") != std::string::npos)
                    return;

                // Simple JSON key extraction (no external JSON dep in this example)
                auto key = std::string("\"command\"");
                auto pos = payload.find(key);
                std::string command;
                if (pos != std::string::npos)
                {
                    auto colon = payload.find(':', pos + key.size());
                    auto q1 = payload.find('"', colon + 1);
                    auto q2 = payload.find('"', q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        command = payload.substr(q1 + 1, q2 - q1 - 1);
                }

                if (command.empty())
                {
                    adapter->publish_command_message(twin_uuid, R"({"status":"error"})");
                    return;
                }

                try
                {
                    if (command == "greetings")
                    {
                        std::cout << "Hello World!\n";
                        adapter->publish_command_message(twin_uuid, R"({"status":"ok"})");
                    }
                    else
                    {
                        adapter->publish_command_message(twin_uuid, R"({"status":"error"})");
                    }
                }
                catch (...)
                {
                    adapter->publish_command_message(twin_uuid, R"({"status":"error"})");
                }
            });

        std::cout << "Subscribed to command messages for twin: " << twin_uuid << "\n";
        std::cout << R"(Send command: {"command": "greetings"})" << "\n";
        std::cout << "Press Ctrl+C to stop...\n\n";

        while (g_running)
            std::this_thread::sleep_for(std::chrono::seconds(1));

        client.disconnect();
        adapter->disconnect();
        std::cout << "\nCommand receiver stopped\n";
    }
    catch (const cyberwave::CyberwaveError& e)
    {
        std::cerr << "SDK error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
