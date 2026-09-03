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
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace cyberwave;

// Release builds define NDEBUG, which compiles <cassert> away — an assert-only
// test then passes no matter what it observes. Used by the health test below;
// the older tests here are left on assert() rather than flipped blind.
static void check(bool ok, const char* what)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::abort();
    }
}

// Mock MQTT for streaming tests
struct MockMqttForStreaming : IMqttClient
{
    bool connected = true;
    std::string topic_prefix_;
    int publish_count = 0;
    mutable std::mutex payload_mutex;
    std::vector<std::string> payloads;
    bool is_connected() const override { return connected; }
    std::string get_topic_prefix() const override { return topic_prefix_; }
    void update_joint_state(const std::string&, const std::string&, double) override {}
    void publish(const std::string& topic, const std::string& json_payload) override
    {
        (void)topic;
        std::lock_guard<std::mutex> lock(payload_mutex);
        payloads.push_back(json_payload);
        ++publish_count;
    }
    void subscribe(const std::string&, MqttMessageHandler) override {}

    /** Most recent edge_health payload, or empty if none yet. */
    std::string last_health() const
    {
        std::lock_guard<std::mutex> lock(payload_mutex);
        for (auto it = payloads.rbegin(); it != payloads.rend(); ++it)
        {
            if (it->find("\"edge_health\"") != std::string::npos)
                return *it;
        }
        return std::string();
    }

    /** Count edge_health heartbeats only — the mock also sees WebRTC signaling. */
    int health_publish_count() const
    {
        std::lock_guard<std::mutex> lock(payload_mutex);
        int n = 0;
        for (const auto& p : payloads)
        {
            if (p.find("\"edge_health\"") != std::string::npos)
                ++n;
        }
        return n;
    }
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

static void test_encoded_h264_publishes_health_without_frames()
{
    // Frames are pushed in from outside, so publishing edge_health from
    // send_frame() meant the heartbeat died with the source. The invariant:
    // heartbeats exist with zero send_frame() calls.
    auto mqtt = std::make_shared<MockMqttForStreaming>();
    mqtt->topic_prefix_ = "";
    EncodedH264CameraStreamer streamer(mqtt, "twin-1");
    streamer.set_log_callback([](const std::string&) {});
    streamer.start();
    if (!streamer.running())
    {
        return; // No WebRTC adapter in this build; nothing to assert.
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    check(mqtt->health_publish_count() >= 1, "heartbeat with zero send_frame() calls");

    // No frame yet is "connecting", not "connected" — the dashboard reads the
    // latter as frames flowing, and "disconnected" would flash red on start.
    const std::string beat = mqtt->last_health();
    check(beat.find("\"connection_state\":\"connecting\"") != std::string::npos, "startup reads connecting");
    check(beat.find("\"is_stale\":false") != std::string::npos, "startup is not stale");

    // stop() must wake the health thread rather than wait out its interval.
    const auto t0 = std::chrono::steady_clock::now();
    streamer.stop();
    const auto stop_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    check(stop_ms < 2000, "stop() is prompt");

    const int at_stop = mqtt->health_publish_count();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    check(mqtt->health_publish_count() == at_stop, "health thread does not outlive stop()");
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
    test_encoded_h264_publishes_health_without_frames();
    test_camera_twin_streaming_with_source();
    test_camera_twin_start_without_source_throws();
    return 0;
}
