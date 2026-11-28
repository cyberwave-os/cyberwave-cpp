/**
 * Test edge node framework (5.1): EdgeNodeConfig, BaseEdgeNode lifecycle.
 * No live backend; no Boost; native C++.
 */

#include "cyberwave/client.h"
#include "cyberwave/edge/base_edge_node.h"
#include "cyberwave/edge/edge_config.h"
#include "cyberwave/exceptions.h"

#include <cpprest/json.h>

#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace cyberwave;

// --- EdgeNodeConfig ---------------------------------------------------------

static void test_config_from_env_validate()
{
    EdgeNodeConfig c = EdgeNodeConfig::from_env();
    // validate() throws if api_key or edge_uuid missing (empty when not set)
    bool threw = false;
    try
    {
        c.validate();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    // If env has CYBERWAVE_API_KEY and EDGE_UUID, validate passes
    assert(c.health_publish_interval_sec >= 1 || c.health_publish_interval_sec == 0);
    (void)threw;
}

static void test_config_validate_throws()
{
    EdgeNodeConfig c;
    c.cyberwave_api_key = "";
    c.edge_uuid = "edge-123";
    bool threw = false;
    try
    {
        c.validate();
    }
    catch (const CyberwaveError& e)
    {
        threw = true;
        assert(std::string(e.what()).find("CYBERWAVE_API_KEY") != std::string::npos);
    }
    assert(threw);

    c.cyberwave_api_key = "key";
    c.edge_uuid = "";
    threw = false;
    try
    {
        c.validate();
    }
    catch (const CyberwaveError& e)
    {
        threw = true;
        assert(std::string(e.what()).find("EDGE_UUID") != std::string::npos);
    }
    assert(threw);
}

static void test_config_to_sdk_config()
{
    EdgeNodeConfig c;
    c.cyberwave_base_url = "https://example.com";
    c.cyberwave_api_key = "test-key";
    c.edge_uuid = "edge-uuid";
    c.mqtt_port = 1883;
    Config sdk = c.to_sdk_config();
    assert(sdk.base_url == "https://example.com");
    assert(sdk.api_key == "test-key");
    assert(sdk.source_type == "edge");
}

// --- Mock MQTT client (implements IMqttClient for tests) ---------------------

struct MockMqttClient : IMqttClient
{
    bool connected = true;
    std::string topic_prefix_;
    bool is_connected() const override { return connected; }
    std::string get_topic_prefix() const override { return topic_prefix_; }
    void update_joint_state(const std::string&, const std::string&, double) override {}
    void publish(const std::string& topic, const std::string& json_payload) override
    {
        std::lock_guard<std::mutex> lock(published_mutex);
        published.emplace_back(topic, json_payload);
    }
    void subscribe(const std::string&, MqttMessageHandler) override {}

    /** The one message published so far, parsed; fails if there is not exactly one. */
    std::pair<std::string, web::json::value> only_message()
    {
        std::lock_guard<std::mutex> lock(published_mutex);
        assert(published.size() == 1);
        return {published[0].first, web::json::value::parse(utility::conversions::to_string_t(published[0].second))};
    }

    std::mutex published_mutex;
    std::vector<std::pair<std::string, std::string>> published;
};

static const web::json::value& field(const web::json::value& payload, const std::string& key)
{
    return payload.at(utility::conversions::to_string_t(key));
}

static std::string string_field(const web::json::value& payload, const std::string& key)
{
    return utility::conversions::to_utf8string(field(payload, key).as_string());
}

static bool has_field(const web::json::value& payload, const std::string& key)
{
    return payload.has_field(utility::conversions::to_string_t(key));
}

// --- Minimal node (exits main_loop immediately so run() returns) ------------

struct MinimalNode : BaseEdgeNode
{
    bool setup_called = false;
    bool subscribe_called = false;
    bool cleanup_called = false;

    MinimalNode(const EdgeNodeConfig& config, Client& client) : BaseEdgeNode(config, client) {}

    void setup() override { setup_called = true; }
    void subscribe_to_commands() override { subscribe_called = true; }
    void cleanup() override { cleanup_called = true; }
    HealthStatus build_health_status() override
    {
        HealthStatus h;
        h["status"] = "ok";
        return h;
    }

    void main_loop() override
    {
        // Exit immediately so run() returns (no infinite loop).
        request_shutdown();
    }
};

static void test_base_edge_node_lifecycle()
{
    EdgeNodeConfig config;
    config.cyberwave_api_key = "test-key";
    config.edge_uuid = "edge-1";
    config.twin_uuid = "twin-1";
    config.cyberwave_base_url = "https://example.com";
    config.health_publish_interval_sec = 5;
    config.validate();

    Config sdk = config.to_sdk_config();
    Client client(sdk);
    auto mqtt = std::make_shared<MockMqttClient>();
    mqtt->topic_prefix_ = "";
    client.set_mqtt_client(mqtt);

    MinimalNode node(config, client);
    node.run();

    assert(node.setup_called);
    assert(node.subscribe_called);
    assert(node.cleanup_called);
    assert(!node.running());
}

static void test_get_twin_uuids()
{
    EdgeNodeConfig config;
    config.cyberwave_api_key = "k";
    config.edge_uuid = "e1";
    config.twin_uuid = "twin-one";
    config.validate();
    Config sdk = config.to_sdk_config();
    Client client(sdk);
    MinimalNode node(config, client);
    std::vector<std::string> uuids = node.get_twin_uuids();
    assert(uuids.size() == 1);
    assert(uuids[0] == "twin-one");
}

static void test_publish_no_crash_without_mqtt()
{
    EdgeNodeConfig config;
    config.cyberwave_api_key = "k";
    config.edge_uuid = "e1";
    config.validate();
    Config sdk = config.to_sdk_config();
    Client client(sdk);
    // No MQTT set - publish_* must be no-op
    MinimalNode node(config, client);
    node.publish_position("twin-1", 0, 0, 0);
    node.publish_health("twin-1", {{"x", "y"}});
    node.publish_event("twin-1", "test", {});
}

// The recording pipeline refuses a message without source_type, and the
// edge_health contract (the backend's TwinEdgeHealthPayload, which Python's
// publish_health sends) names the heartbeat's fields; these pin both for the
// C++ publishers.

static EdgeNodeConfig recording_config()
{
    EdgeNodeConfig config;
    config.cyberwave_api_key = "k";
    config.edge_uuid = "edge-7";
    config.source_type = SOURCE_TYPE_SIM;
    config.validate();
    return config;
}

static void test_publish_health_matches_the_edge_health_contract()
{
    EdgeNodeConfig config = recording_config();
    Client client(config.to_sdk_config());
    auto mqtt = std::make_shared<MockMqttClient>();
    mqtt->topic_prefix_ = "dev/";
    client.set_mqtt_client(mqtt);
    MinimalNode node(config, client);

    // A node's own health fields ride along, but cannot replace the identity
    // fields the pipeline attributes the heartbeat by.
    node.publish_health("twin-1", {{"status", "ok"},
                                   {"type", "custom"},
                                   {"source_type", "tele"},
                                   {"edge_id", "spoofed"},
                                   {"twin_uuid", "twin-2"},
                                   {"uptime_seconds", "forever"}});

    auto [topic, payload] = mqtt->only_message();
    assert(topic == "dev/cyberwave/twin/twin-1/edge_health");
    assert(string_field(payload, "type") == "edge_health");
    assert(string_field(payload, "source_type") == SOURCE_TYPE_SIM);
    assert(string_field(payload, "edge_id") == "edge-7");
    assert(string_field(payload, "twin_uuid") == "twin-1");
    assert(string_field(payload, "status") == "ok");
    assert(field(payload, "timestamp").is_number());
    const double uptime = field(payload, "uptime_seconds").as_double();
    assert(uptime >= 0.0 && uptime < 60.0);
    assert(std::fabs(uptime * 10.0 - std::round(uptime * 10.0)) < 1e-6);
    // The names this publisher used before the contract.
    assert(!has_field(payload, "edge_uuid"));
    assert(!has_field(payload, "uptime"));
    // A whole-second uptime is still spelled as a float, as Python sends it.
    const std::string& raw = mqtt->published[0].second;
    const auto at = raw.find("\"uptime_seconds\":");
    assert(at != std::string::npos);
    const std::string uptime_text = raw.substr(at + 17, raw.find_first_of(",}", at) - at - 17);
    assert(uptime_text.find_first_of(".e") != std::string::npos);
}

static void test_publish_health_stamps_edge_for_an_unknown_source_type()
{
    EdgeNodeConfig config = recording_config();
    config.source_type = "sim_typo";
    Client client(config.to_sdk_config());
    auto mqtt = std::make_shared<MockMqttClient>();
    client.set_mqtt_client(mqtt);
    MinimalNode node(config, client);

    node.publish_health("twin-1", {});

    auto [topic, payload] = mqtt->only_message();
    assert(string_field(payload, "source_type") == SOURCE_TYPE_EDGE);
}

static void test_publish_nav_status_carries_source_type()
{
    EdgeNodeConfig config = recording_config();
    Client client(config.to_sdk_config());
    auto mqtt = std::make_shared<MockMqttClient>();
    client.set_mqtt_client(mqtt);
    MinimalNode node(config, client);

    node.publish_nav_status("twin-1", "action-3", "running", std::string("halfway"), 0.5);

    auto [topic, payload] = mqtt->only_message();
    assert(topic == "cyberwave/twin/twin-1/navigate/status");
    assert(string_field(payload, "source_type") == SOURCE_TYPE_SIM);
    assert(string_field(payload, "action_id") == "action-3");
    assert(string_field(payload, "status") == "running");
    assert(string_field(payload, "message") == "halfway");
    assert(field(payload, "progress").as_double() == 0.5);
    assert(field(payload, "timestamp").is_number());
}

/** Every message the node published so far, as (topic, parsed payload). */
static std::vector<std::pair<std::string, web::json::value>> all_messages(MockMqttClient& mqtt)
{
    std::lock_guard<std::mutex> lock(mqtt.published_mutex);
    std::vector<std::pair<std::string, web::json::value>> out;
    for (const auto& [topic, payload] : mqtt.published)
        out.emplace_back(topic, web::json::value::parse(utility::conversions::to_string_t(payload)));
    return out;
}

/** Publishes one message on each of the node's attributable topics. */
static void publish_one_of_each(MinimalNode& node)
{
    node.publish_position("twin-1", 1.0, 2.0, 3.0, std::array<double, 4>{0.0, 0.0, 0.0, 1.0});
    node.publish_joint_states("twin-1", {{"j1", 0.5}});
    node.publish_nav_status("twin-1", "action-1", "running");
    node.publish_health("twin-1", {});
    GpsFix fix;
    fix.latitude = 45.0;
    fix.longitude = 9.0;
    node.publish_gps("twin-1", fix);
    node.publish_event("twin-1", "object_detected", {{"label", "box"}});
    node.publish_telemetry("twin-1", "battery", {{"level", "80"}});
}

static void test_publish_event_carries_source_type()
{
    EdgeNodeConfig config = recording_config();
    Client client(config.to_sdk_config());
    auto mqtt = std::make_shared<MockMqttClient>();
    client.set_mqtt_client(mqtt);
    MinimalNode node(config, client);

    node.publish_event("twin-1", "object_detected", {{"label", "box"}});

    auto [topic, payload] = mqtt->only_message();
    assert(topic == "cyberwave/twin/twin-1/event");
    assert(string_field(payload, "source_type") == SOURCE_TYPE_SIM);
    assert(string_field(payload, "source") == "edge_node");
    assert(string_field(payload, "event_type") == "object_detected");
    assert(string_field(field(payload, "data"), "label") == "box");
}

// An empty or unknown configured source_type is stamped as `edge` on every
// publisher, through one helper, as the heartbeat already did.
static void test_every_publisher_stamps_edge_for_an_unusable_source_type()
{
    for (const std::string configured : {"sim_typo", ""})
    {
        EdgeNodeConfig config = recording_config();
        config.source_type = configured;
        Client client(config.to_sdk_config());
        auto mqtt = std::make_shared<MockMqttClient>();
        client.set_mqtt_client(mqtt);
        MinimalNode node(config, client);

        publish_one_of_each(node);

        const auto messages = all_messages(*mqtt);
        assert(messages.size() == 8); // position and rotation, then one each
        for (const auto& [topic, payload] : messages)
        {
            assert(has_field(payload, "source_type"));
            assert(string_field(payload, "source_type") == SOURCE_TYPE_EDGE);
        }
    }
}

// A valid configured value is stamped as is, except on nav_status, which
// mirrors Python's publish_nav_status: leader/follower report as `edge`, and
// `tele`/`edit` send no source_type (the backend lets only `edge`, or none,
// move a physical navigation action).
static void test_publish_nav_status_maps_source_type_as_python_does()
{
    const std::pair<const char*, const char*> cases[] = {
        {SOURCE_TYPE_EDGE_LEADER, SOURCE_TYPE_EDGE},
        {SOURCE_TYPE_EDGE_FOLLOWER, SOURCE_TYPE_EDGE},
        {SOURCE_TYPE_EDGE, SOURCE_TYPE_EDGE},
        {SOURCE_TYPE_SIM_TELE, SOURCE_TYPE_SIM_TELE},
        {SOURCE_TYPE_TELE, nullptr},
        {SOURCE_TYPE_EDIT, nullptr},
    };
    for (const auto& [configured, expected] : cases)
    {
        EdgeNodeConfig config = recording_config();
        config.source_type = configured;
        Client client(config.to_sdk_config());
        auto mqtt = std::make_shared<MockMqttClient>();
        client.set_mqtt_client(mqtt);
        MinimalNode node(config, client);

        node.publish_nav_status("twin-1", "action-1", "running");
        node.publish_event("twin-1", "object_detected", {});

        const auto messages = all_messages(*mqtt);
        assert(messages.size() == 2);
        const auto& nav = messages[0].second;
        if (expected)
            assert(string_field(nav, "source_type") == expected);
        else
            assert(!has_field(nav, "source_type"));
        // Other publishers stamp the configured value unchanged.
        assert(string_field(messages[1].second, "source_type") == configured);
    }
}

int main()
{
    test_config_from_env_validate();
    test_config_validate_throws();
    test_config_to_sdk_config();
    test_base_edge_node_lifecycle();
    test_get_twin_uuids();
    test_publish_no_crash_without_mqtt();
    test_publish_health_matches_the_edge_health_contract();
    test_publish_health_stamps_edge_for_an_unknown_source_type();
    test_publish_nav_status_carries_source_type();
    test_publish_event_carries_source_type();
    test_every_publisher_stamps_edge_for_an_unusable_source_type();
    test_publish_nav_status_maps_source_type_as_python_does();
    return 0;
}
