/**
 * Tests for IMqttClient default helper implementations.
 * Mirrors Python mqtt client helper tests.
 * Verifies that the default implementations produce the correct topics/payloads
 * and that subscribe helpers call subscribe() with the right topics.
 */

#include <cyberwave/mqtt_interface.h>

#include <cassert>
#include <map>
#include <string>
#include <vector>

using namespace cyberwave;

/** Concrete stub implementing only the pure-virtual methods. */
struct StubMqttClient : public IMqttClient
{
    bool connected = true;
    std::string prefix = "test/";

    struct Call
    {
        std::string topic;
        std::string payload;
    };
    std::vector<Call> publishes;
    std::vector<Call> subscribes;

    bool is_connected() const override { return connected; }
    std::string get_topic_prefix() const override { return prefix; }

    void update_joint_state(const std::string&, const std::string&, double) override {}

    void publish(const std::string& topic, const std::string& payload) override
    {
        publishes.push_back({topic, payload});
    }

    void subscribe(const std::string& topic, MqttMessageHandler) override { subscribes.push_back({topic, ""}); }
};

/** update_twin_position produces correct topic and JSON payload */
static void test_update_twin_position_topic_and_payload()
{
    StubMqttClient m;
    m.update_twin_position("twin-abc", 1.0, 2.0, 3.0);
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-abc/position");
    const std::string& payload = m.publishes[0].payload;
    assert(payload.find("\"x\":1") != std::string::npos);
    assert(payload.find("\"y\":2") != std::string::npos);
    assert(payload.find("\"z\":3") != std::string::npos);
}

/** update_twin_rotation produces correct topic and JSON payload */
static void test_update_twin_rotation_topic_and_payload()
{
    StubMqttClient m;
    m.update_twin_rotation("twin-abc", 1.0, 0.0, 0.0, 0.0);
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-abc/rotation");
    const std::string& payload = m.publishes[0].payload;
    assert(payload.find("\"w\":1") != std::string::npos);
}

/** update_twin_scale produces correct topic */
static void test_update_twin_scale_topic()
{
    StubMqttClient m;
    m.update_twin_scale("twin-abc", 2.0, 2.0, 2.0);
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-abc/scale");
}

/** update_joints_state produces correct topic and payload */
static void test_update_joints_state_topic_and_payload()
{
    StubMqttClient m;
    std::map<std::string, double> joints;
    joints["elbow"] = 1.57;
    joints["shoulder"] = 0.5;
    m.update_joints_state("twin-abc", joints, "sdk");
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-abc/joint_states");
    const std::string& payload = m.publishes[0].payload;
    assert(payload.find("elbow") != std::string::npos);
    assert(payload.find("shoulder") != std::string::npos);
    assert(payload.find("source_type") != std::string::npos);
}

/** subscribe_twin uses wildcard topic */
static void test_subscribe_twin_wildcard_topic()
{
    StubMqttClient m;
    m.subscribe_twin("twin-abc", [](const std::string&) {});
    assert(m.subscribes.size() == 1);
    assert(m.subscribes[0].topic == "test/cyberwave/twin/twin-abc/#");
}

/** subscribe_twin_position uses correct topic */
static void test_subscribe_twin_position_topic()
{
    StubMqttClient m;
    m.subscribe_twin_position("twin-abc", [](const std::string&) {});
    assert(m.subscribes.size() == 1);
    assert(m.subscribes[0].topic == "test/cyberwave/twin/twin-abc/position");
}

/** subscribe_twin_rotation uses correct topic */
static void test_subscribe_twin_rotation_topic()
{
    StubMqttClient m;
    m.subscribe_twin_rotation("twin-abc", [](const std::string&) {});
    assert(m.subscribes.size() == 1);
    assert(m.subscribes[0].topic == "test/cyberwave/twin/twin-abc/rotation");
}

/** subscribe_twin_joint_states uses correct topic */
static void test_subscribe_twin_joint_states_topic()
{
    StubMqttClient m;
    m.subscribe_twin_joint_states("twin-abc", [](const std::string&) {});
    assert(m.subscribes.size() == 1);
    assert(m.subscribes[0].topic == "test/cyberwave/twin/twin-abc/joint_states");
}

/** update_joints_state with velocities/efforts uses aggregated format */
static void test_update_joints_state_aggregated()
{
    StubMqttClient m;
    std::map<std::string, double> pos = {{"j1", 1.0}};
    std::map<std::string, double> vel = {{"j1", 0.5}};
    std::map<std::string, double> eff = {{"j1", 0.1}};
    m.update_joints_state("twin-abc", pos, "sdk", vel, eff, 1234.0, "wl-uuid", "sess-1");
    assert(m.publishes.size() == 1);
    const std::string& p = m.publishes[0].payload;
    assert(p.find("velocities") != std::string::npos);
    assert(p.find("efforts") != std::string::npos);
    assert(p.find("timestamp") != std::string::npos);
    assert(p.find("workload_uuid") != std::string::npos);
    assert(p.find("session_id") != std::string::npos);
}

/** publish_telemetry_start uses correct topic */
static void test_publish_telemetry_start_topic()
{
    StubMqttClient m;
    m.publish_telemetry_start("twin-abc");
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-abc/telemetry");
    assert(m.publishes[0].payload.find("telemetry_start") != std::string::npos);
}

/** publish_initial_observation uses correct topic and fps */
static void test_publish_initial_observation_topic()
{
    StubMqttClient m;
    m.publish_initial_observation("twin-abc", "{\"obs\":1}", 60.0);
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-abc/telemetry");
    assert(m.publishes[0].payload.find("initial_observation") != std::string::npos);
    assert(m.publishes[0].payload.find("60") != std::string::npos);
}

/** subscribe_environment uses wildcard topic */
static void test_subscribe_environment_topic()
{
    StubMqttClient m;
    m.subscribe_environment("env-abc", [](const std::string&) {});
    assert(m.subscribes.size() == 1);
    assert(m.subscribes[0].topic == "test/cyberwave/environment/env-abc/+");
}

/** publish_environment_update uses update_type in topic */
static void test_publish_environment_update_topic()
{
    StubMqttClient m;
    m.publish_environment_update("env-abc", "twin_added", "{\"id\":\"t1\"}");
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/environment/env-abc/twin_added");
}

/** Stream subscribe helpers use correct topics */
static void test_stream_subscribe_topics()
{
    StubMqttClient m;
    m.subscribe_video_stream("twin-1", [](const std::string&) {});
    m.subscribe_depth_stream("twin-2", [](const std::string&) {});
    m.subscribe_pointcloud_stream("twin-3", [](const std::string&) {});
    assert(m.subscribes[0].topic == "test/cyberwave/twin/twin-1/video");
    assert(m.subscribes[1].topic == "test/cyberwave/twin/twin-2/depth");
    assert(m.subscribes[2].topic == "test/cyberwave/twin/twin-3/pointcloud");
}

/** WebRTC and command message pub/sub topics */
static void test_webrtc_and_command_topics()
{
    StubMqttClient m;
    m.publish_webrtc_message("twin-1", "{\"type\":\"offer\",\"sdp\":\"...\"}");
    m.subscribe_webrtc_messages("twin-1", [](const std::string&) {});
    m.publish_command_message("twin-1", "{\"status\":\"ok\"}");
    m.subscribe_command_message("twin-1", [](const std::string&) {});
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-1/webrtc-offer");
    assert(m.subscribes[0].topic == "test/cyberwave/twin/twin-1/webrtc-offer");
    assert(m.subscribes[1].topic == "test/cyberwave/twin/twin-1/webrtc-answer");
    assert(m.subscribes[2].topic == "test/cyberwave/twin/twin-1/webrtc-candidate");
    assert(m.publishes[1].topic == "test/cyberwave/twin/twin-1/command");
    assert(m.subscribes[3].topic == "test/cyberwave/twin/twin-1/command");

    auto scoped = m.subscribe_webrtc_messages_scoped("twin-2", [](const std::string&) {});
    assert(m.subscribes.size() == 6);
    assert(m.subscribes[3].topic == "test/cyberwave/twin/twin-2/webrtc-offer");
    scoped.reset();
}

/** Ping / pong topics */
static void test_ping_pong_topics()
{
    StubMqttClient m;
    m.ping("res-123");
    m.subscribe_pong("res-123", [](const std::string&) {});
    assert(m.publishes[0].topic == "test/cyberwave/ping/res-123");
    assert(m.subscribes[0].topic == "test/cyberwave/pong/res-123");
}

/** Empty prefix works correctly (no separator doubled) */
static void test_empty_prefix()
{
    StubMqttClient m;
    m.prefix = "";
    m.update_twin_position("twin-xyz", 0.0, 0.0, 0.0);
    assert(m.publishes[0].topic == "cyberwave/twin/twin-xyz/position");
}

/** source_subtype appears in aggregated payload */
static void test_update_joints_state_source_subtype()
{
    StubMqttClient m;
    std::map<std::string, double> pos = {{"joint1", 1.0}};
    m.update_joints_state("twin-x", pos, "edge", {}, {}, -1.0, "", "", "openvla");
    assert(!m.publishes.empty());
    const std::string& payload = m.publishes[0].payload;
    assert(payload.find("source_subtype") != std::string::npos);
    assert(payload.find("openvla") != std::string::npos);
}

/** connect/disconnect are no-ops by default */
static void test_connect_disconnect_noop()
{
    StubMqttClient m;
    m.connect();                 // should not throw
    m.disconnect();              // should not throw
    assert(m.publishes.empty()); // no side effects
}

/** update_joint_state rich overload falls back to 3-arg version */
static void test_update_joint_state_rich_overload()
{
    struct TrackingMqttClient : public StubMqttClient
    {
        bool called_3arg = false;
        using IMqttClient::update_joint_state; // expose 7-arg default overload
        void update_joint_state(const std::string& twin_uuid, const std::string& joint_name,
                                double position_rad) override
        {
            called_3arg = true;
            StubMqttClient::update_joint_state(twin_uuid, joint_name, position_rad);
        }
    };

    TrackingMqttClient m;
    // 7-arg overload should fall back to 3-arg
    m.update_joint_state("twin", "j1", 0.5, 0.0, 0.0, 1234.0, "edge");
    assert(m.called_3arg);
}

int main()
{
    test_update_twin_position_topic_and_payload();
    test_update_twin_rotation_topic_and_payload();
    test_update_twin_scale_topic();
    test_update_joints_state_topic_and_payload();
    test_update_joints_state_aggregated();
    test_subscribe_twin_wildcard_topic();
    test_subscribe_twin_position_topic();
    test_subscribe_twin_rotation_topic();
    test_subscribe_twin_joint_states_topic();
    test_publish_telemetry_start_topic();
    test_publish_initial_observation_topic();
    test_subscribe_environment_topic();
    test_publish_environment_update_topic();
    test_stream_subscribe_topics();
    test_webrtc_and_command_topics();
    test_ping_pong_topics();
    test_empty_prefix();
    test_update_joints_state_source_subtype();
    test_connect_disconnect_noop();
    test_update_joint_state_rich_overload();
    return 0;
}
