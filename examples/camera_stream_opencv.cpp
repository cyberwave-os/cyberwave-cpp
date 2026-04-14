/*
    camera_stream_opencv.cpp — OpenCV camera source example

    Demonstrates:
     - Implementing IFrameSource using OpenCV VideoCapture
     - Streaming raw BGR frames via CameraTwin/CameraStreamer
     - Optional JPEG fallback payload for legacy /video consumers

    Before running:
        export CYBERWAVE_API_KEY=your_token_here
        export CYBERWAVE_MQTT_HOST=mqtt.cyberwave.com
        export TWIN_UUID=your-camera-twin-uuid

    Requires:
     - MQTT example deps (PahoMqttCpp + nlohmann_json + spdlog)
     - OpenCV (core,imgproc,videoio)
*/

#include <cyberwave/camera_streaming.h>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/paho_mqtt_adapter.h>
#include <cyberwave/twin_subclasses.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

static volatile bool g_running = true;

class OpenCvFrameSource final : public cyberwave::IFrameSource
{
public:
    explicit OpenCvFrameSource(const int device_index)
    {
        cap_.open(device_index);
        if (!cap_.isOpened())
        {
            throw std::runtime_error("Failed to open camera device index " + std::to_string(device_index));
        }
    }

    bool next_frame(cyberwave::VideoFrame& frame_out) override
    {
        cv::Mat bgr;
        if (!cap_.read(bgr) || bgr.empty())
        {
            return false;
        }

        frame_out.width = bgr.cols;
        frame_out.height = bgr.rows;
        frame_out.pixel_format = cyberwave::PixelFormat::BGR24;
        frame_out.timestamp =
            std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        frame_out.data.assign(bgr.data, bgr.data + (static_cast<std::size_t>(bgr.rows) * bgr.step));

        // Optional: provide JPEG bytes for MQTT /video fallback mode.
        std::vector<unsigned char> jpeg;
        const std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 85};
        if (cv::imencode(".jpg", bgr, jpeg, params))
        {
            frame_out.jpeg_fallback = std::move(jpeg);
        }
        else
        {
            frame_out.jpeg_fallback.clear();
        }
        return true;
    }

private:
    cv::VideoCapture cap_;
};

int main()
{
    std::signal(SIGINT, [](int) { g_running = false; });

    try
    {
        cyberwave::Config cfg;
        cfg.load_from_environment();
        cyberwave::Client cw(cfg);

        auto adapter = std::make_shared<cyberwave::PahoMqttAdapter>(cfg);
        adapter->connect();
        cw.set_mqtt_client(adapter);

        std::string twin_uuid;
        if (const char* env = std::getenv("TWIN_UUID"))
        {
            twin_uuid = env;
        }
        else
        {
            std::cerr << "TWIN_UUID is required for this example.\n";
            return 1;
        }

        const int camera_index = (std::getenv("CAMERA_INDEX") != nullptr) ? std::atoi(std::getenv("CAMERA_INDEX")) : 0;
        const int fps = (std::getenv("CAMERA_FPS") != nullptr) ? std::atoi(std::getenv("CAMERA_FPS")) : 30;

        cyberwave::CameraTwin camera(cw, twin_uuid, "camera");
        auto source = std::make_shared<OpenCvFrameSource>(camera_index);
        camera.set_frame_source(source);

        camera.start_streaming(fps);
        std::cout << "Streaming OpenCV camera index " << camera_index << " at " << fps
                  << " FPS... Press Ctrl+C to stop.\n";

        while (g_running)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        camera.stop_streaming();
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
