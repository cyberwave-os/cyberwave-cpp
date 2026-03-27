/*
    go2_locomotion.cpp — Unitree Go2 locomotion example

    Mirrors: examples/go2_locomotion.py

    Demonstrates:
     - Using affect() to direct commands to the real robot or simulation
     - Wiring the real Paho MQTT client via PahoMqttAdapter
     - Sending a move_forward command via LocomoteTwin

    Before running:
        export CYBERWAVE_API_KEY=your_token_here
        export CYBERWAVE_MQTT_HOST=mqtt.cyberwave.com
        export TWIN_UUID=your-go2-twin-uuid   # or leave unset to use first twin

    Requires: cyberwave_sdk + cyberwave_mqtt_client (PahoMqttCpp)
*/

#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/paho_mqtt_adapter.h>
#include <cyberwave/twin_subclasses.h>
#include <cyberwave/twins.h>

#include <cstdlib>
#include <iostream>
#include <memory>

int main()
{
    try
    {
        cyberwave::Config cfg;
        cfg.load_from_environment();
        cyberwave::Client cw(cfg);

        // Default to simulation so the example is safe in CI and ad-hoc smoke runs.
        // Set CYBERWAVE_AFFECT=real-world to intentionally target a physical robot.
        const char* affect_mode = std::getenv("CYBERWAVE_AFFECT");
        cw.affect(affect_mode && *affect_mode ? affect_mode : "simulation");

        // Wire the real MQTT client. Mirrors Python: Cyberwave(mqtt_host=...) auto-connects.
        auto adapter = std::make_shared<cyberwave::PahoMqttAdapter>(cfg);
        adapter->connect();
        cw.set_mqtt_client(adapter);

        // Resolve twin UUID
        std::string twin_uuid;
        if (const char* env = std::getenv("TWIN_UUID"))
        {
            twin_uuid = env;
        }
        else
        {
            auto twins = cw.twins().list();
            if (twins.empty())
            {
                std::cout << "No twins found. Set TWIN_UUID or create a twin first.\n";
                return 0;
            }
            twin_uuid = twins[0].uuid();
            std::cout << "Using twin: " << twins[0].name() << " (" << twin_uuid << ")\n";
        }

        cyberwave::LocomoteTwin robot(cw, twin_uuid, "go2");
        robot.move_forward(1.0);
        std::cout << "move_forward sent (source_type: " << cw.source_type() << ")\n";

        cw.disconnect();
        adapter->disconnect();
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
