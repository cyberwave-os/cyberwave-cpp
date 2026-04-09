/*
    quickstart.cpp — Cyberwave C++ SDK Quick Start

    Mirrors: examples/quickstart.py

    Demonstrates:
     - Creating a twin handle and editing its position / rotation
     - Reading and setting joint positions
     - Using affect() to target simulation vs. real robot
     - Subscribing to real-time position and joint updates (requires MQTT)

    Before running:
        export CYBERWAVE_BASE_URL=http://localhost:8000
        export CYBERWAVE_API_KEY=your_token_here

    For MQTT subscriptions set additionally:
        export CYBERWAVE_MQTT_HOST=mqtt.example.com
*/

#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/joints.h>
#include <cyberwave/twin.h>
#include <cyberwave/twin_subclasses.h>
#include <cyberwave/twins.h>
#ifdef CYBERWAVE_HAVE_MQTT
#include <cyberwave/cyberwave_mqtt_adapter.h>
#endif

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main()
{
    try
    {
        cyberwave::Config cfg;
        cfg.load_from_environment();
        cyberwave::Client cw(cfg);
#ifdef CYBERWAVE_HAVE_MQTT
        std::shared_ptr<cyberwave::CyberwaveMqttAdapter> mqtt_adapter;
        if (!cfg.mqtt_host.empty())
        {
            mqtt_adapter = std::make_shared<cyberwave::CyberwaveMqttAdapter>(cfg);
            mqtt_adapter->connect();
            cw.set_mqtt_client(mqtt_adapter);
        }
#endif

        // Discover available twins and use the first one
        auto twins = cw.twins().list();
        if (twins.empty())
        {
            std::cout << "No twins found. Create one in the Cyberwave dashboard first.\n";
            return 0;
        }
        const std::string& twin_uuid = twins[0].uuid();
        std::cout << "Using twin: " << twins[0].name() << " (" << twin_uuid << ")\n";

        // --- Edit a twin's position and rotation in the environment ---
        cyberwave::Twin robot = twins[0];
        robot.edit_position(1.0, 0.0, 0.5);
        robot.edit_rotation(90.0); // yaw overload: degrees

        // --- Control joints (skip if twin has no URDF / no joint_states) ---
        try
        {
            auto joints = robot.joints().get_all();
            if (!joints.empty())
            {
                const std::string& first_joint = joints.begin()->first;
                std::cout << "Setting joint '" << first_joint << "' to 30°\n";
                robot.joints().set(first_joint, 30.0, /*degrees=*/true);
                std::cout << "Joint '" << first_joint << "': " << robot.joints().get(first_joint) << " rad\n";
            }
        }
        catch (const cyberwave::CyberwaveError&)
        {
            std::cout << "Twin has no joint states (no URDF), skipping joint demo\n";
        }

        // --- affect(): direct commands to simulation or real robot ---
        // "simulation" → moves the digital twin in Cyberwave
        // "real-world" → moves the physical robot
        cw.affect("simulation");
        std::cout << "Source type: " << cw.source_type() << "\n";

        // --- Real-time updates via MQTT ---
        if (cw.mqtt_client() && cw.mqtt_client()->is_connected())
        {
            robot.subscribe_position([](const std::string& data) { std::cout << "Position: " << data << "\n"; });
            robot.subscribe_joints([](const std::string& data) { std::cout << "Joints: " << data << "\n"; });
        }
        else
        {
            std::cout << "Skipping MQTT subscriptions (no connected MQTT client configured)\n";
        }

        robot.edit_position(2.0, 1.0, 0.5);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        cw.disconnect();
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
