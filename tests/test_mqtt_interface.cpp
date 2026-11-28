/**
 * Tests for IMqttClient default helper implementations.
 * Mirrors Python mqtt client helper tests.
 * Verifies that the default implementations produce the correct topics/payloads
 * and that subscribe helpers call subscribe() with the right topics.
 */

#include <cyberwave/mqtt_interface.h>

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <locale>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cyberwave;

// Release builds define NDEBUG, which compiles <cassert> away. check() predates
// the -UNDEBUG that CMakeLists.txt now puts on the test targets; with it, every
// assert() in this file runs in the Release ctest run too.
static void check(bool ok, const char* what)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::abort();
    }
}

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

/** A client configured with a source type, as a simulator or teleop leader is. */
struct ConfiguredStub : StubMqttClient
{
    std::string configured = SOURCE_TYPE_SIM;
    std::string default_source_type() const override { return configured; }
};

/**
 * `payload` is `head` followed by a float `timestamp` close to now.
 *
 * The recording pipeline rejects a pose it cannot attribute to a
 * `source_type`, and reads the timestamp's type from its spelling.
 */
static void assert_pose_message(const std::string& payload, const std::string& head)
{
    const std::string stamped = head + ",\"timestamp\":";
    assert(payload.rfind(stamped, 0) == 0);
    assert(payload.back() == '}');
    const std::string number = payload.substr(stamped.size(), payload.size() - stamped.size() - 1);
    assert(number.find_first_of(".eE") != std::string::npos);
    std::istringstream in(number);
    in.imbue(std::locale::classic());
    double seconds = 0.0;
    const bool parsed = static_cast<bool>(in >> seconds);
    assert(parsed && in.eof());
    assert(std::fabs(seconds - wall_seconds()) < 60.0);
}

/** update_twin_position produces correct topic and JSON payload */
static void test_update_twin_position_topic_and_payload()
{
    StubMqttClient m;
    m.update_twin_position("twin-abc", 1.0, 2.0, 3.0);
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-abc/position");
    assert_pose_message(m.publishes[0].payload,
                        R"({"source_type":"edge","type":"position","position":{"x":1.0,"y":2.0,"z":3.0})");
}

/** update_twin_rotation produces correct topic and JSON payload */
static void test_update_twin_rotation_topic_and_payload()
{
    StubMqttClient m;
    m.update_twin_rotation("twin-abc", 1.0, 0.0, 0.0, 0.0);
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-abc/rotation");
    assert_pose_message(m.publishes[0].payload,
                        R"({"source_type":"edge","type":"rotation","rotation":{"w":1.0,"x":0.0,"y":0.0,"z":0.0})");
}

/** update_twin_scale produces correct topic and JSON payload */
static void test_update_twin_scale_topic()
{
    StubMqttClient m;
    m.update_twin_scale("twin-abc", 2.0, 2.0, 2.5);
    assert(m.publishes.size() == 1);
    assert(m.publishes[0].topic == "test/cyberwave/twin/twin-abc/scale");
    assert_pose_message(m.publishes[0].payload,
                        R"({"source_type":"edge","type":"scale","scale":{"x":2.0,"y":2.0,"z":2.5})");
}

/** The pose helpers take no source_type, so they send the client's. */
static void test_pose_helpers_send_the_clients_source_type()
{
    ConfiguredStub m;
    m.update_twin_position("twin-abc", 1.0, 2.0, 3.0);
    m.update_twin_rotation("twin-abc", 1.0, 0.0, 0.0, 0.0);
    m.update_twin_scale("twin-abc", 1.0, 1.0, 1.0);
    m.configured = "";
    m.update_twin_position("twin-abc", 1.0, 2.0, 3.0);
    assert(m.publishes.size() == 4);
    for (int i = 0; i < 3; ++i)
        assert(m.publishes[i].payload.rfind(R"({"source_type":"sim",)", 0) == 0);
    assert(m.publishes[3].payload.rfind(R"({"source_type":"edge",)", 0) == 0);
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

/**
 * update_joints_state always sends a source_type: the caller's, else the client's.
 *
 * Its default argument is empty, and a message with no source_type is one the
 * recording pipeline rejects.
 */
static void test_update_joints_state_always_sends_a_source_type()
{
    ConfiguredStub m;
    const std::map<std::string, double> positions = {{"elbow", 1.0}};
    m.update_joints_state("twin-abc", positions);
    m.update_joints_state("twin-abc", positions, SOURCE_TYPE_TELE);
    m.configured = "";
    m.update_joints_state("twin-abc", positions, "", {}, {}, 1234.5);
    assert(m.publishes.size() == 3);
    assert(m.publishes[0].payload == R"({"source_type":"sim","positions":{"elbow":1.0}})");
    assert(m.publishes[1].payload == R"({"source_type":"tele","positions":{"elbow":1.0}})");
    assert(m.publishes[2].payload == R"({"source_type":"edge","timestamp":1234.5,"positions":{"elbow":1.0}})");
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
    assert(m.subscribes.size() == 7);
    assert(m.subscribes[4].topic == "test/cyberwave/twin/twin-2/webrtc-offer");
    assert(m.subscribes[5].topic == "test/cyberwave/twin/twin-2/webrtc-answer");
    assert(m.subscribes[6].topic == "test/cyberwave/twin/twin-2/webrtc-candidate");
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

/** accelerations is its own channel, published under `accelerations` when given */
static void test_update_joints_state_accelerations()
{
    StubMqttClient m;
    std::map<std::string, double> pos = {{"j1", 1.0}};
    std::map<std::string, double> acc = {{"j1", 0.25}, {"j2", 0.0}};
    // Appended last: every earlier positional argument binds as it always has.
    m.update_joints_state("twin-abc", pos, "sdk", {}, {}, -1.0, "", "", "", acc);
    check(m.publishes.size() == 1, "one publish");
    const std::string& p = m.publishes[0].payload;
    check(p.find("\"positions\":{\"j1\":1.0}") != std::string::npos, "positions published");
    // An explicit 0.0 is a real measurement and is kept.
    check(p.find("\"accelerations\":{\"j1\":0.25,\"j2\":0.0}") != std::string::npos, "accelerations published");
    check(p.find("efforts") == std::string::npos, "no efforts fabricated from accelerations");
    check(p.find("velocities") == std::string::npos, "no velocities fabricated");
    check(p.find("target_") == std::string::npos, "measured state is never a target");
}

/** no accelerations given: the key is absent, and efforts never stand in for it */
static void test_update_joints_state_without_accelerations_omits_the_key()
{
    StubMqttClient m;
    std::map<std::string, double> pos = {{"j1", 1.0}};
    std::map<std::string, double> eff = {{"j1", 0.1}};
    m.update_joints_state("twin-abc", pos, "sdk");
    m.update_joints_state("twin-abc", pos, "sdk", {}, eff);
    check(m.publishes.size() == 2, "two publishes");
    check(m.publishes[0].payload.find("acceleration") == std::string::npos, "positions-only has no accelerations");
    check(m.publishes[1].payload.find("\"efforts\":{\"j1\":0.1}") != std::string::npos, "efforts published");
    check(m.publishes[1].payload.find("acceleration") == std::string::npos, "efforts never aliased to accelerations");
}

/** joint targets: a command under the target_ keys, on the joint update topic */
static void test_update_joint_targets_accelerations_only()
{
    StubMqttClient m;
    std::map<std::string, double> acc = {{"j1", 1.5}, {"j2", 0.0}};
    // No position target: acceleration alone is a valid command.
    m.update_joint_targets("twin-abc", {}, "sim_tele", {}, {}, 1234.5, "", "", "", acc);
    check(m.publishes.size() == 1, "one target publish");
    check(m.publishes[0].topic == "test/cyberwave/joint/twin-abc/update", "targets go to the joint update topic");
    // An explicit 0.0 commands that channel; nothing else is fabricated.
    check(m.publishes[0].payload ==
              "{\"source_type\":\"sim_tele\",\"target_accelerations\":{\"j1\":1.5,\"j2\":0.0},\"timestamp\":1234.5}",
          "accelerations-only target payload");
}

/** every target channel and the metadata, each under its own key */
static void test_update_joint_targets_every_channel()
{
    StubMqttClient m;
    m.update_joint_targets("twin-abc", {{"j1", 1.0}}, "sim_tele", {{"j1", 0.5}}, {{"j1", 0.1}}, 1234.5, "wl", "sess",
                           "openvla", {{"j1", 2.0}});
    check(m.publishes.size() == 1, "one target publish");
    check(m.publishes[0].payload == "{\"source_type\":\"sim_tele\",\"target_positions\":{\"j1\":1.0},"
                                    "\"target_velocities\":{\"j1\":0.5},\"target_accelerations\":{\"j1\":2.0},"
                                    "\"target_efforts\":{\"j1\":0.1},\"timestamp\":1234.5,\"workload_uuid\":\"wl\","
                                    "\"session_id\":\"sess\",\"source_subtype\":\"openvla\"}",
          "full target payload");
}

/** efforts-only targets never carry an acceleration, and vice versa */
static void test_update_joint_targets_never_alias_efforts_and_accelerations()
{
    StubMqttClient m;
    m.update_joint_targets("twin-abc", {}, "sim_tele", {}, {{"j1", 0.4}}, 1.0);
    m.update_joint_targets("twin-abc", {}, "sim_tele", {}, {}, 1.0, "", "", "", {{"j1", 0.4}});
    check(m.publishes.size() == 2, "two target publishes");
    check(m.publishes[0].payload.find("\"target_efforts\":{\"j1\":0.4}") != std::string::npos, "effort target");
    check(m.publishes[0].payload.find("acceleration") == std::string::npos, "effort target has no acceleration");
    check(m.publishes[1].payload.find("\"target_accelerations\":{\"j1\":0.4}") != std::string::npos,
          "acceleration target");
    check(m.publishes[1].payload.find("effort") == std::string::npos, "acceleration target has no effort");
    for (const auto& call : m.publishes)
    {
        check(call.payload.find("\"positions\"") == std::string::npos, "a target is never measured positions");
        check(call.payload.find("target_positions") == std::string::npos, "no position target fabricated");
        check(call.payload.find("target_velocities") == std::string::npos, "no velocity target fabricated");
    }
}

/** no target at all is refused; a missing timestamp is stamped with the current time */
static void test_update_joint_targets_rejects_empty_and_stamps_time()
{
    StubMqttClient m;
    bool threw = false;
    try
    {
        m.update_joint_targets("twin-abc", {});
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    check(threw, "a target with no channel is refused");
    check(m.publishes.empty(), "nothing published for an empty target");

    m.update_joint_targets("twin-abc", {{"j1", 1.0}});
    check(m.publishes.size() == 1, "position-only target published");
    const std::string& p = m.publishes[0].payload;
    check(p.rfind("{\"source_type\":\"edge\",\"target_positions\":{\"j1\":1.0},\"timestamp\":", 0) == 0,
          "empty source_type falls back to edge, as Python's; timestamp stamped");
    const double stamped = std::stod(p.substr(p.find("\"timestamp\":") + 12));
    check(stamped > 1.6e9, "timestamp is the current time");
}

/** the hand-written JSON stays valid: escaped names, round-trip numbers, null for non-finite */
static void test_update_joint_targets_json_encoding()
{
    StubMqttClient m;
    m.update_joint_targets("twin-abc", {{"a\"b\\c\n", 0.1}, {"neg", -1e-300}, {"nan", std::nan("")}}, "", {}, {}, 0.0);
    check(m.publishes.size() == 1, "one target publish");
    check(m.publishes[0].payload ==
              "{\"source_type\":\"edge\",\"target_positions\":{\"a\\\"b\\\\c\\n\":0.1,\"nan\":null,\"neg\":-1e-300},"
              "\"timestamp\":0.0}",
          "escaped joint names, shortest round-trip numbers, null for NaN");
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

// --- JSON spelling ----------------------------------------------------------------

/** Every float spelled as JSON that parses back to the same double, and stays a float. */
static void test_json_float()
{
    // floats stay floats, even when whole
    assert(detail::json_float(3.0) == "3.0");
    assert(detail::json_float(-0.0) == "-0.0");
    assert(detail::json_float(12.5) == "12.5");
    assert(detail::json_float(0.1) == "0.1");
    assert(detail::json_float(1e20) == "1e+20");
    assert(detail::json_float(1788433200.25) == "1788433200.25");
    // the shortest digits that round-trip, as Python's repr()
    assert(detail::json_float(1.0 / 3.0) == "0.3333333333333333");
    assert(std::strtod(detail::json_float(1.0 / 3.0).c_str(), nullptr) == 1.0 / 3.0);
    assert(detail::json_float(0.1 + 0.2) == "0.30000000000000004");
    assert(detail::json_float(5e-324) == "5e-324");
    assert(std::strtod(detail::json_float(std::numeric_limits<double>::max()).c_str(), nullptr) ==
           std::numeric_limits<double>::max());
    // fixed from 1e-4 up to 1e16, scientific outside, as Python's repr()
    assert(detail::json_float(100000.0) == "100000.0");
    assert(detail::json_float(1e15) == "1000000000000000.0");
    assert(detail::json_float(1e16) == "1e+16");
    assert(detail::json_float(0.0001) == "0.0001");
    assert(detail::json_float(0.00001) == "1e-05");
    // JSON has no NaN or infinity
    assert(detail::json_float(std::nan("")) == "null");
    assert(detail::json_float(std::numeric_limits<double>::infinity()) == "null");
    assert(detail::json_float(-std::numeric_limits<double>::infinity()) == "null");
}

/** Strings, escaped. */
static void test_json_quote()
{
    assert(detail::json_quote("docked") == R"("docked")");
    assert(detail::json_quote("a\"b\\c") == R"("a\"b\\c")");
    assert(detail::json_quote("line\nfeed\ttab") == R"("line\nfeed\ttab")");
    assert(detail::json_quote(std::string("\x01\x1f", 2)) == R"("\u0001\u001f")");
    assert(detail::json_quote("temp\xC3\xA9rature") == "\"temp\xC3\xA9rature\"");
    assert(detail::json_quote("\xE2\x82\xAC \xF0\x9F\xA4\x96") == "\"\xE2\x82\xAC \xF0\x9F\xA4\x96\"");
    // bytes outside a well-formed UTF-8 sequence become U+FFFD, one each
    const std::string replacement = "\\ufffd";
    assert(detail::json_quote("a\xFF"
                              "b") == "\"a" + replacement + "b\"");                    // never valid
    assert(detail::json_quote("\x80") == "\"" + replacement + "\"");                   // stray continuation
    assert(detail::json_quote("\xC3") == "\"" + replacement + "\"");                   // truncated at the end
    assert(detail::json_quote("\xC3(") == "\"" + replacement + "(\"");                 // truncated mid-string
    assert(detail::json_quote("\xC0\xAF") == "\"" + replacement + replacement + "\""); // overlong '/'
    assert(detail::json_quote("\xED\xA0\x80") == "\"" + replacement + replacement + replacement + "\""); // surrogate
    assert(detail::json_quote("\xF4\x90\x80\x80") ==
           "\"" + replacement + replacement + replacement + replacement + "\""); // above U+10FFFF
}

/** A float formats the same whatever the global locale: no decimal commas, no digit grouping. */
static void test_json_float_ignores_the_global_locale()
{
    // Built here rather than looked up (de_DE may not be installed), so the
    // test cannot quietly skip itself.
    struct CommaDecimal : std::numpunct<char>
    {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
    };
    const std::locale previous = std::locale::global(std::locale(std::locale::classic(), new CommaDecimal));
    const std::string fractional = detail::json_float(12.5);
    const std::string large = detail::json_float(1234567.5);
    const std::string whole = detail::json_float(1234567.0);
    // A locale-aware round-trip check would read "0,1" back wrongly and
    // fall back to 17 digits, 0.10000000000000001.
    const std::string shortest = detail::json_float(0.1);
    StubMqttClient m;
    m.update_joints_state("twin-abc", {{"j1", 1234567.5}});
    std::locale::global(previous);
    assert(fractional == "12.5");
    assert(large == "1234567.5");
    assert(whole == "1234567.0");
    assert(shortest == "0.1");
    assert(m.publishes[0].payload == R"({"source_type":"edge","positions":{"j1":1234567.5}})");
}

/** The rich single-joint overload carries acceleration as its own last, defaulted channel */
static void test_update_joint_state_rich_overload_carries_acceleration()
{
    struct RecordingMqttClient : public StubMqttClient
    {
        using StubMqttClient::update_joint_state;
        std::optional<double> velocity, effort, acceleration;
        void update_joint_state(const std::string&, const std::string&, double, std::optional<double> v,
                                std::optional<double> e, double = -1.0, const std::string& = "",
                                std::optional<double> a = std::nullopt) override
        {
            velocity = v;
            effort = e;
            acceleration = a;
        }
    };

    RecordingMqttClient m;
    IMqttClient& client = m; // dispatch through the interface, as callers do
    client.update_joint_state("twin", "j1", 0.5, std::nullopt, std::nullopt, 1234.0, "edge", 0.0);
    check(m.acceleration.has_value() && *m.acceleration == 0.0, "explicit zero acceleration reaches the client");
    check(!m.effort.has_value(), "acceleration never stands in for effort");
    check(!m.velocity.has_value(), "no velocity fabricated");

    client.update_joint_state("twin", "j1", 0.5, 0.2, 1.5);
    check(!m.acceleration.has_value(), "omitted acceleration stays not-commanded");
    check(m.effort.has_value() && *m.effort == 1.5, "effort unchanged by the new parameter");

    // The default implementation still falls back to the 3-arg overload.
    struct TrackingMqttClient : public StubMqttClient
    {
        bool called_3arg = false;
        using IMqttClient::update_joint_state;
        void update_joint_state(const std::string&, const std::string&, double) override { called_3arg = true; }
    };
    TrackingMqttClient t;
    t.update_joint_state("twin", "j1", 0.5, std::nullopt, std::nullopt, -1.0, "", 2.0);
    check(t.called_3arg, "default rich overload with acceleration falls back to 3-arg");
}

/** The same default as the Python SDK, and the limit on Cyberwave's brokers. */
static void test_the_default_payload_limit_is_32_mib()
{
    static_assert(DEFAULT_MQTT_MAX_PAYLOAD_BYTES == 33554432);
    assert(std::string(MQTT_MAX_PAYLOAD_BYTES_ENV) == "CYBERWAVE_MQTT_MAX_PAYLOAD_BYTES");
}

/** Vector's size_guard_colored_pointcloud limit, as the Python SDK's POINTCLOUD_INGEST_MAX_PAYLOAD_BYTES. */
static void test_only_point_clouds_over_8_mib_go_unrecorded()
{
    static_assert(MQTT_POINTCLOUD_RECORDED_MAX_PAYLOAD_BYTES == 8388608);
    const std::string cloud = "cyberwave/twin/8f1c2d3e-0000-4000-8000-000000000001/pointcloud";
    assert(!mqtt_publish_unrecorded(cloud, MQTT_POINTCLOUD_RECORDED_MAX_PAYLOAD_BYTES));
    assert(mqtt_publish_unrecorded(cloud, MQTT_POINTCLOUD_RECORDED_MAX_PAYLOAD_BYTES + 1));
    assert(!mqtt_publish_unrecorded("cyberwave/twin/8f1c2d3e-0000-4000-8000-000000000001/depth",
                                    MQTT_POINTCLOUD_RECORDED_MAX_PAYLOAD_BYTES + 1));
    assert(!mqtt_publish_unrecorded("pointcloud", MQTT_POINTCLOUD_RECORDED_MAX_PAYLOAD_BYTES + 1));
}

static void test_parse_mqtt_max_payload_bytes()
{
    assert(parse_mqtt_max_payload_bytes("4096") == std::optional<std::size_t>(4096));
    assert(parse_mqtt_max_payload_bytes(" 4096\n") == std::optional<std::size_t>(4096));
    assert(parse_mqtt_max_payload_bytes("0") == std::optional<std::size_t>(0));
    // Kept in step with the malformed values in the Python SDK's test_mqtt_payload_limit.py.
    for (const char* bad : {"", "   ", "32MiB", "-1", "+1", "1.5", "4096 bytes", "0\v", "\u00a00",
                            "18446744073709551616", "99999999999999999999999"})
    {
        assert(!parse_mqtt_max_payload_bytes(bad).has_value());
    }
}

static void test_mqtt_payload_fits()
{
    assert(mqtt_payload_fits(64, 64));
    assert(!mqtt_payload_fits(65, 64));
    assert(mqtt_payload_fits(0, 64));
    // 0 is no limit, not a limit of nothing.
    assert(mqtt_payload_fits(DEFAULT_MQTT_MAX_PAYLOAD_BYTES + 1, 0));
    assert(!mqtt_payload_fits(DEFAULT_MQTT_MAX_PAYLOAD_BYTES + 1, DEFAULT_MQTT_MAX_PAYLOAD_BYTES));
    // Never past what mosquitto_publish's int length can hold, with or without a limit.
    assert(mqtt_payload_fits(MQTT_MAX_PUBLISHABLE_PAYLOAD_BYTES, 0));
    assert(!mqtt_payload_fits(MQTT_MAX_PUBLISHABLE_PAYLOAD_BYTES + 1, 0));
    assert(!mqtt_payload_fits(MQTT_MAX_PUBLISHABLE_PAYLOAD_BYTES + 1, MQTT_MAX_PUBLISHABLE_PAYLOAD_BYTES + 2));
    assert(mqtt_effective_max_payload_bytes(64) == 64);
    assert(mqtt_effective_max_payload_bytes(0) == MQTT_MAX_PUBLISHABLE_PAYLOAD_BYTES);
}

static void test_report_throttle_lets_one_report_per_key_through_per_interval()
{
    using namespace std::chrono_literals;
    detail::ReportThrottle throttle(10s);
    const auto t0 = std::chrono::steady_clock::time_point{} + 1h;

    assert(throttle.admit("a", t0) == std::optional<std::size_t>(0));
    assert(!throttle.admit("a", t0 + 1s).has_value());
    assert(!throttle.admit("a", t0 + 9s).has_value());
    // Each key has its own interval.
    assert(throttle.admit("b", t0 + 9s) == std::optional<std::size_t>(0));
    // The next report carries the count held back before it.
    assert(throttle.admit("a", t0 + 10s) == std::optional<std::size_t>(2));
    assert(!throttle.admit("a", t0 + 11s).has_value());
    assert(throttle.admit("a", t0 + 30s) == std::optional<std::size_t>(1));
}

static void test_report_throttle_reports_a_first_use_at_the_clocks_epoch()
{
    // A zero time_point is a real steady_clock reading, not "never reported".
    detail::ReportThrottle throttle(std::chrono::seconds{10});
    const std::chrono::steady_clock::time_point epoch{};

    assert(throttle.admit("a", epoch) == std::optional<std::size_t>(0));
    assert(!throttle.admit("a", epoch).has_value());
}

static void test_report_throttle_forgets_the_least_recently_reported_key()
{
    using namespace std::chrono_literals;
    detail::ReportThrottle throttle(10s, 2);
    const auto t0 = std::chrono::steady_clock::time_point{} + 1h;

    assert(throttle.admit("a", t0) == std::optional<std::size_t>(0));
    assert(throttle.admit("b", t0 + 1s) == std::optional<std::size_t>(0));
    assert(!throttle.admit("a", t0 + 2s).has_value());
    // A third key evicts "a", the least recently reported.
    assert(throttle.admit("c", t0 + 3s) == std::optional<std::size_t>(0));
    assert(throttle.size() == 2);
    assert(!throttle.admit("b", t0 + 4s).has_value());
    // "a" was forgotten, so it is let through again (evicting "b").
    assert(throttle.admit("a", t0 + 4s) == std::optional<std::size_t>(0));
    assert(throttle.size() == 2);
}

static void test_report_throttle_stays_bounded_under_many_keys()
{
    detail::ReportThrottle throttle(std::chrono::seconds{10});
    const auto t0 = std::chrono::steady_clock::time_point{};
    for (std::size_t i = 0; i < detail::ReportThrottle::kDefaultMaxKeys * 4; ++i)
    {
        throttle.admit("topic/" + std::to_string(i), t0 + std::chrono::milliseconds(i));
    }
    assert(throttle.size() == detail::ReportThrottle::kDefaultMaxKeys);
}

int main()
{
    test_update_twin_position_topic_and_payload();
    test_update_twin_rotation_topic_and_payload();
    test_update_twin_scale_topic();
    test_pose_helpers_send_the_clients_source_type();
    test_update_joints_state_topic_and_payload();
    test_update_joints_state_always_sends_a_source_type();
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
    test_update_joints_state_accelerations();
    test_update_joints_state_without_accelerations_omits_the_key();
    test_update_joint_targets_accelerations_only();
    test_update_joint_targets_every_channel();
    test_update_joint_targets_never_alias_efforts_and_accelerations();
    test_update_joint_targets_rejects_empty_and_stamps_time();
    test_update_joint_targets_json_encoding();
    test_connect_disconnect_noop();
    test_update_joint_state_rich_overload();
    test_json_float();
    test_json_quote();
    test_json_float_ignores_the_global_locale();
    test_update_joint_state_rich_overload_carries_acceleration();
    test_the_default_payload_limit_is_32_mib();
    test_parse_mqtt_max_payload_bytes();
    test_only_point_clouds_over_8_mib_go_unrecorded();
    test_mqtt_payload_fits();
    test_report_throttle_lets_one_report_per_key_through_per_interval();
    test_report_throttle_reports_a_first_use_at_the_clocks_epoch();
    test_report_throttle_forgets_the_least_recently_reported_key();
    test_report_throttle_stays_bounded_under_many_keys();
    return 0;
}
