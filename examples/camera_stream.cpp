/*
    camera_stream.cpp — Camera streaming example

    Mirrors: examples/camera_stream.py

    Demonstrates:
     - Wiring the MQTT client via CyberwaveMqttAdapter
     - Streaming synthetic frames to a digital twin via CameraTwin
     - Stopping on Ctrl+C

    To stream from a real camera:
     - Implement IFrameSource to pull frames from your capture device
       (OpenCV, V4L2, GStreamer, etc.) and pass it to set_frame_source().

    Before running:
        export CYBERWAVE_API_KEY=your_token_here
        export CYBERWAVE_MQTT_HOST=mqtt.cyberwave.com
        export TWIN_UUID=your-camera-twin-uuid

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
                std::cout << "No twins found. Set TWIN_UUID or create a camera twin.\n";
                return 0;
            }
            twin_uuid = twins[0].uuid();
            std::cout << "Using twin: " << twins[0].name() << " (" << twin_uuid << ")\n";
        }

        // CameraTwin bundles camera streaming capabilities.
        cyberwave::CameraTwin camera(cw, twin_uuid, "camera");

        // VirtualFrameSource generates synthetic JPEG frames for testing.
        // Replace with your own IFrameSource for a real camera.
        auto source = std::make_shared<cyberwave::VirtualFrameSource>();
        camera.set_frame_source(source);

        camera.start_streaming(/*fps=*/30);
        std::cout << "Streaming... Press Ctrl+C to stop.\n";

        while (g_running)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        camera.stop_streaming();
        std::cout << "\nStreaming stopped.\n";

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
