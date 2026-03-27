/**
 * Test edge node framework (5.1): EdgeNodeConfig, BaseEdgeNode lifecycle.
 * No live backend; no Boost; native C++.
 */

#include "cyberwave/client.h"
#include "cyberwave/edge/base_edge_node.h"
#include "cyberwave/edge/edge_config.h"
#include "cyberwave/exceptions.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <thread>

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
    void publish(const std::string&, const std::string&) override {}
    void subscribe(const std::string&, MqttMessageHandler) override {}
};

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

int main()
{
    test_config_from_env_validate();
    test_config_validate_throws();
    test_config_to_sdk_config();
    test_base_edge_node_lifecycle();
    test_get_twin_uuids();
    test_publish_no_crash_without_mqtt();
    return 0;
}
