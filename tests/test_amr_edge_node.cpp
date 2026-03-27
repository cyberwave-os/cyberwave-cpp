/**
 * Test AMR edge node (5.2): AdapterConfig, RobotState, RobotTelemetry, AMREdgeNode.
 */

#include "cyberwave/client.h"
#include "cyberwave/edge/amr_adapter.h"
#include "cyberwave/edge/amr_edge_node.h"
#include "cyberwave/edge/amr_types.h"
#include "cyberwave/edge/edge_config.h"

#include <cassert>
#include <memory>

using namespace cyberwave;

// --- Mock adapter ------------------------------------------------------------

struct MockAMRAdapter : IAMRAdapter
{
    bool connected = false;
    AMRStatusCallback status_callback;

    void connect() override { connected = true; }
    void disconnect() override { connected = false; }
    bool is_connected() const override { return connected; }

    std::optional<RobotTelemetry> poll_telemetry() override
    {
        RobotTelemetry t;
        t.position = Position3{1.0, 2.0, 3.0};
        t.state = RobotState::Idle;
        t.battery_level = 85.0;
        t.battery_charging = false;
        return t;
    }

    bool send_navigation_command(const std::string& action_id, const std::string& command,
                                 std::optional<Position3> position = std::nullopt,
                                 std::optional<RotationQuat> rotation = std::nullopt,
                                 std::vector<Position3> waypoints = {}) override
    {
        (void)position;
        (void)rotation;
        (void)waypoints;
        if (status_callback)
            status_callback(action_id, "running", std::nullopt, std::nullopt);
        return true;
    }
    bool cancel_navigation(const std::string& action_id) override
    {
        (void)action_id;
        return true;
    }
    bool pause_navigation() override { return true; }
    bool resume_navigation() override { return true; }
    void set_status_callback(AMRStatusCallback cb) override { status_callback = std::move(cb); }
};

// --- Minimal AMR node (exits main_loop immediately) ---------------------------

struct MinimalAMRNode : AMREdgeNode
{
    int discover_calls = 0;

    MinimalAMRNode(const EdgeNodeConfig& config, Client& client, std::optional<AdapterConfig> ac = std::nullopt)
        : AMREdgeNode(config, client, ac)
    {
    }
    std::shared_ptr<IAMRAdapter> create_adapter() override { return std::make_shared<MockAMRAdapter>(); }
    void discover_twins() override
    {
        ++discover_calls;
        discovered_twin_uuids_ = {config_.twin_uuid};
    }
    void main_loop() override { request_shutdown(); }
};

// --- Tests -------------------------------------------------------------------

static void test_robot_state_to_string()
{
    assert(std::string(to_string(RobotState::Idle)) == "idle");
    assert(std::string(to_string(RobotState::Navigating)) == "navigating");
    assert(std::string(to_string(NavigationStatus::Queued)) == "queued");
    assert(std::string(to_string(NavigationStatus::Completed)) == "completed");
}

static void test_adapter_config_from_env()
{
    AdapterConfig c = AdapterConfig::from_env();
    assert(c.position_poll_rate_hz > 0.0);
    assert(c.telemetry_poll_rate_hz > 0.0);
}

static void test_amr_edge_node_lifecycle()
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
    MinimalAMRNode node(config, client);
    node.run();

    assert(node.adapter());
    assert(node.adapter()->is_connected());
    assert(node.discover_calls == 1);
    assert(!node.running());
}

static void test_amr_adapter_config_access()
{
    EdgeNodeConfig config;
    config.cyberwave_api_key = "k";
    config.edge_uuid = "e1";
    config.twin_uuid = "t1";
    config.validate();
    Config sdk = config.to_sdk_config();
    Client client(sdk);
    MinimalAMRNode node(config, client);
    assert(node.adapter_config().position_poll_rate_hz > 0.0);
    assert(node.adapter_config().telemetry_poll_rate_hz > 0.0);
}

int main()
{
    test_robot_state_to_string();
    test_adapter_config_from_env();
    test_amr_edge_node_lifecycle();
    test_amr_adapter_config_access();
    return 0;
}
