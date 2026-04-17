/*
    depth_stream.cpp — Depth camera streaming example

    Mirrors: examples/realsense_stream.py

    Demonstrates:
     - Wiring the MQTT client via CyberwaveMqttAdapter
     - Streaming synthetic depth frames to a digital twin via DepthCameraTwin
     - Stopping on Ctrl+C

    To stream from a real Intel RealSense (or any depth camera):
     - Implement IDepthSource to pull depth frames from the device
       and pass it to set_depth_source().

    Before running:
        export CYBERWAVE_API_KEY=your_token_here
        export CYBERWAVE_MQTT_HOST=mqtt.cyberwave.com
        export TWIN_UUID=your-depth-camera-twin-uuid

    Requires: cyberwave_sdk + cyberwave_mqtt_client (libmosquitto)
*/

#include <cyberwave/camera_streaming.h>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/cyberwave_mqtt_adapter.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/twin_subclasses.h>
#include <cyberwave/twins.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

static volatile bool g_running = true;

int main()
{
    std::signal(SIGINT, [](int) { g_running = false; });

    try
    {
        cyberwave::Config cfg;
        cfg.load_from_environment();
        cyberwave::Client cw(cfg);

        // Wire the real MQTT client. Mirrors Python: Cyberwave(mqtt_host=...) auto-connects.
        auto adapter = std::make_shared<cyberwave::CyberwaveMqttAdapter>(cfg);
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
                std::cout << "No twins found. Set TWIN_UUID or create a depth camera twin.\n";
                return 0;
            }
            twin_uuid = twins[0].uuid();
            std::cout << "Using twin: " << twins[0].name() << " (" << twin_uuid << ")\n";
        }

        // DepthCameraTwin adds depth streaming on top of RGB streaming.
        cyberwave::DepthCameraTwin camera(cw, twin_uuid, "depth-camera");

        // VirtualDepthSource generates synthetic depth frames for testing.
        // Replace with a RealSense wrapper implementing IDepthSource for real hardware.
        auto depth_source = std::make_shared<cyberwave::VirtualDepthSource>();
        camera.set_depth_source(depth_source);

        camera.start_depth_streaming(/*fps=*/10);
        std::cout << "Depth streaming... Press Ctrl+C to stop.\n";

        while (g_running)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        camera.stop_depth_streaming();
        std::cout << "\nDepth streaming stopped.\n";

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
