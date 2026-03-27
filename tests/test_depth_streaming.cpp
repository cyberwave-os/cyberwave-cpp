/**
 * Test depth streaming (5.4): IDepthSource, VirtualDepthSource,
 * DepthStreamer, DepthCameraTwin depth streaming + publish helpers.
 */

#include "cyberwave/camera_streaming.h"
#include "cyberwave/client.h"
#include "cyberwave/config.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twin_subclasses.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace cyberwave;

struct MockMqttForDepth : IMqttClient
{
    bool connected = true;
    std::string topic_prefix_;
    int depth_publishes = 0;
    int pointcloud_publishes = 0;
    int other_publishes = 0;

    bool is_connected() const override { return connected; }
    std::string get_topic_prefix() const override { return topic_prefix_; }
    void update_joint_state(const std::string&, const std::string&, double) override {}
    void publish(const std::string& topic, const std::string& json_payload) override
    {
        (void)json_payload;
        if (topic.find("/depth") != std::string::npos)
            ++depth_publishes;
        else if (topic.find("/pointcloud") != std::string::npos)
            ++pointcloud_publishes;
        else
            ++other_publishes;
    }
    void subscribe(const std::string&, MqttMessageHandler) override {}
};

// Depth source that also produces a tiny point cloud
struct TestDepthSource : IDepthSource
{
    bool next_depth_frame(DepthFrame& frame_out) override
    {
        frame_out.width = 2;
        frame_out.height = 2;
        frame_out.timestamp = 1.0;
        frame_out.data = {500, 600, 700, 800};
        return true;
    }
    bool next_point_cloud(std::vector<PointXYZRGB>& cloud_out) override
    {
        cloud_out.push_back({0.1f, 0.2f, 1.5f, 255, 0, 0});
        return true;
    }
};

static void test_virtual_depth_source()
{
    VirtualDepthSource src;
    DepthFrame frame;
    assert(src.next_depth_frame(frame));
    assert(frame.width == 4 && frame.height == 4);
    assert(frame.data.size() == 16);
    assert(frame.data[0] == 1000); // 1 m flat plane
    assert(frame.timestamp > 0.0);
}

static void test_depth_streamer_publishes_depth()
{
    auto src = std::make_shared<TestDepthSource>();
    auto mqtt = std::make_shared<MockMqttForDepth>();
    mqtt->topic_prefix_ = "";
    mqtt->connected = true;
    DepthStreamer streamer(mqtt, "twin-d", src, 10);
    assert(!streamer.running());
    streamer.start();
    assert(streamer.running());
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    streamer.stop();
    assert(!streamer.running());
    assert(mqtt->depth_publishes >= 1);
    assert(mqtt->pointcloud_publishes >= 1); // TestDepthSource supports point cloud
}

static void test_depth_camera_twin_factory()
{
    Config config;
    config.api_key = "k";
    Client client(config);
    Capabilities caps;
    caps.has_depth = true;
    auto t = create_twin(client, "twin-depth", "depth", caps);
    // Must be DepthCameraTwin (which extends CameraTwin which extends Twin)
    auto* depth = dynamic_cast<DepthCameraTwin*>(t.get());
    assert(depth != nullptr);
    // Also accessible as CameraTwin
    auto* cam = dynamic_cast<CameraTwin*>(t.get());
    assert(cam != nullptr);
}

static void test_depth_camera_twin_streaming()
{
    Config config;
    config.api_key = "k";
    Client client(config);
    auto mqtt = std::make_shared<MockMqttForDepth>();
    mqtt->topic_prefix_ = "";
    client.set_mqtt_client(mqtt);

    Capabilities caps;
    caps.has_depth = true;
    auto t = create_twin(client, "twin-depth", "depth", caps);
    auto* depth = dynamic_cast<DepthCameraTwin*>(t.get());
    assert(depth != nullptr);

    auto src = std::make_shared<TestDepthSource>();
    depth->set_depth_source(src);
    depth->start_depth_streaming(10);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    depth->stop_depth_streaming();
    assert(mqtt->depth_publishes >= 1);
}

static void test_depth_camera_twin_start_without_source_throws()
{
    Config config;
    config.api_key = "k";
    Client client(config);
    auto mqtt = std::make_shared<MockMqttForDepth>();
    client.set_mqtt_client(mqtt);
    Capabilities caps;
    caps.has_depth = true;
    auto t = create_twin(client, "twin-depth", "depth", caps);
    auto* depth = dynamic_cast<DepthCameraTwin*>(t.get());
    assert(depth != nullptr);
    bool threw = false;
    try
    {
        depth->start_depth_streaming();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_depth_camera_twin_manual_publish()
{
    Config config;
    config.api_key = "k";
    Client client(config);
    auto mqtt = std::make_shared<MockMqttForDepth>();
    mqtt->connected = true;
    client.set_mqtt_client(mqtt);

    Capabilities caps;
    caps.has_depth = true;
    auto t = create_twin(client, "twin-depth", "depth", caps);
    auto* depth = dynamic_cast<DepthCameraTwin*>(t.get());
    assert(depth != nullptr);

    depth->publish_depth_frame("{\"type\":\"depth_data\",\"data\":{}}");
    depth->publish_point_cloud("{\"pointcloud\":[]}");
    assert(mqtt->depth_publishes == 1);
    assert(mqtt->pointcloud_publishes == 1);
}

static void test_not_implemented_methods_throw()
{
    Config config;
    config.api_key = "k";
    Client client(config);
    Capabilities caps;
    caps.has_depth = true;
    auto t = create_twin(client, "twin-depth", "depth", caps);
    auto* depth = dynamic_cast<DepthCameraTwin*>(t.get());
    assert(depth != nullptr);

    bool threw_cdf = false;
    try
    {
        depth->capture_depth_frame();
    }
    catch (const CyberwaveError&)
    {
        threw_cdf = true;
    }
    assert(threw_cdf);

    bool threw_gpc = false;
    try
    {
        depth->get_point_cloud();
    }
    catch (const CyberwaveError&)
    {
        threw_gpc = true;
    }
    assert(threw_gpc);
}

int main()
{
    test_virtual_depth_source();
    test_depth_streamer_publishes_depth();
    test_depth_camera_twin_factory();
    test_depth_camera_twin_streaming();
    test_depth_camera_twin_start_without_source_throws();
    test_depth_camera_twin_manual_publish();
    test_not_implemented_methods_throw();
    return 0;
}
