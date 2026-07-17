/**
 * Test camera/sensor streaming (5.3): IFrameSource, VirtualFrameSource,
 * CameraStreamer, CameraTwin::set_frame_source/start_streaming/stop_streaming.
 */

#include "cyberwave/camera_streaming.h"
#include "cyberwave/client.h"
#include "cyberwave/config.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twin_subclasses.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <thread>

using namespace cyberwave;

// Mock MQTT for streaming tests
struct MockMqttForStreaming : IMqttClient
{
    bool connected = true;
    std::string topic_prefix_;
    int publish_count = 0;
    bool is_connected() const override { return connected; }
    std::string get_topic_prefix() const override { return topic_prefix_; }
    void update_joint_state(const std::string&, const std::string&, double) override {}
    void publish(const std::string& topic, const std::string& json_payload) override
    {
        (void)topic;
        (void)json_payload;
        ++publish_count;
    }
    void subscribe(const std::string&, MqttMessageHandler) override {}
};

static void test_virtual_frame_source()
{
    VirtualFrameSource src;
    VideoFrame frame;
    assert(src.next_frame(frame));
    assert(frame.width == 1);
    assert(frame.height == 1);
    assert(frame.pixel_format == PixelFormat::BGR24);
    assert(frame.data.size() == 3);
    assert(frame.jpeg_fallback.size() >= 100);
    assert(frame.jpeg_fallback[0] == 0xff && frame.jpeg_fallback[1] == 0xd8); // JPEG SOI
}

static void test_camera_streamer_start_stop()
{
    auto src = std::make_shared<VirtualFrameSource>();
    auto mqtt = std::make_shared<MockMqttForStreaming>();
    mqtt->topic_prefix_ = "";
    CameraStreamer streamer(mqtt, "twin-1", src, 10);
    assert(!streamer.running());
    streamer.start();
    assert(streamer.running());
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    streamer.stop();
    assert(!streamer.running());
    assert(mqtt->publish_count >= 1);
}

static void test_camera_twin_streaming_with_source()
{
    Config config;
    config.api_key = "k";
    Client client(config);
    auto mqtt = std::make_shared<MockMqttForStreaming>();
    mqtt->topic_prefix_ = "";
    client.set_mqtt_client(mqtt);

    Capabilities caps;
    caps.has_sensors = true;
    auto t = create_twin(client, "twin-cam", "cam", caps);
    auto* cam = dynamic_cast<CameraTwin*>(t.get());
    assert(cam != nullptr);

    auto src = std::make_shared<VirtualFrameSource>();
    cam->set_frame_source(src);
    cam->start_streaming(10, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    cam->stop_streaming();
    assert(mqtt->publish_count >= 1);
}

static void test_encoded_h264_streamer_lifecycle()
{
    auto mqtt = std::make_shared<MockMqttForStreaming>();
    mqtt->topic_prefix_ = "";
    EncodedH264CameraStreamer streamer(mqtt, "twin-1");
    assert(!streamer.running());

    std::vector<std::uint8_t> annexb = {0x00, 0x00, 0x00, 0x01, 0x67};
    assert(!streamer.send_frame(annexb, 0));

    streamer.set_log_callback([](const std::string&) {});
    streamer.start();
    streamer.stop();
    assert(!streamer.running());
    assert(!streamer.send_frame(annexb, 0));
}

static void test_camera_twin_start_without_source_throws()
{
    Config config;
    config.api_key = "k";
    Client client(config);
    auto mqtt = std::make_shared<MockMqttForStreaming>();
    client.set_mqtt_client(mqtt);
    Capabilities caps;
    caps.has_sensors = true;
    auto t = create_twin(client, "twin-cam", "cam", caps);
    auto* cam = dynamic_cast<CameraTwin*>(t.get());
    assert(cam != nullptr);
    bool threw = false;
    try
    {
        cam->start_streaming(30, 0);
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_virtual_frame_source();
    test_camera_streamer_start_stop();
    test_encoded_h264_streamer_lifecycle();
    test_camera_twin_streaming_with_source();
    test_camera_twin_start_without_source_throws();
    return 0;
}
