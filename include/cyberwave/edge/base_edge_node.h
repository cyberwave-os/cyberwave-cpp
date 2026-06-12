/**
 * @brief Base edge-node runtime with lifecycle, health publishing, and MQTT helpers.
 */

#ifndef CYBERWAVE_EDGE_BASE_EDGE_NODE_H
#define CYBERWAVE_EDGE_BASE_EDGE_NODE_H

#include "cyberwave/edge/edge_config.h"
#include "cyberwave/mqtt_interface.h"

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace cyberwave
{

class Client;

/**
 * @brief Base class for Cyberwave edge nodes.
 */
class BaseEdgeNode
{
public:
    /** @brief Health payload represented as string key-value pairs. */
    using HealthStatus = std::map<std::string, std::string>;

    /**
     * @brief Construct a base edge node.
     * @param config Edge-node configuration.
     * @param client SDK client used for REST and optional MQTT operations.
     */
    explicit BaseEdgeNode(const EdgeNodeConfig& config, Client& client);

    /** @brief Virtual destructor. */
    virtual ~BaseEdgeNode();

    BaseEdgeNode(const BaseEdgeNode&) = delete;
    BaseEdgeNode& operator=(const BaseEdgeNode&) = delete;

    /** Run the node (blocking). Connects lifecycle, then runs until shutdown. */
    void run();

    /** Request shutdown (set running to false). Safe to call from another thread. */
    void request_shutdown();

    /** Graceful shutdown: stop health thread and run node cleanup hooks. */
    void shutdown();

    /** @brief Return the current edge-node configuration. */
    const EdgeNodeConfig& config() const noexcept { return config_; }

    /** @brief Return the mutable SDK client bound to this node. */
    Client& client() noexcept { return client_.get(); }

    /** @brief Return the read-only SDK client bound to this node. */
    const Client& client() const noexcept { return client_.get(); }

    /** @brief Return whether the node is currently running. */
    bool running() const noexcept { return running_.load(); }

    // --- Twin discovery (override _discover_twins to populate) ---
    /** Twin UUIDs this node serves (from _discovered_twins or config.twin_uuid). */
    std::vector<std::string> get_twin_uuids() const;

    /** Override to discover twins (e.g. via client->edges().get(edge_uuid)); default no-op. */
    virtual void discover_twins();

    // --- Publish helpers (no-op if client has no MQTT) ---
    /**
     * @brief Publish a pose update for a twin.
     * @param twin_uuid Twin UUID.
     * @param x X position in meters.
     * @param y Y position in meters.
     * @param z Z position in meters.
     * @param rotation_xyzw Optional quaternion in `{x, y, z, w}` storage order used by the implementation.
     */
    void publish_position(const std::string& twin_uuid, double x, double y, double z,
                          std::optional<std::array<double, 4>> rotation_xyzw = std::nullopt);

    /** @brief Publish joint-state telemetry for a twin. */
    void publish_joint_states(const std::string& twin_uuid, const std::map<std::string, double>& joint_states);

    /** @brief Publish navigation status for a twin action. */
    void publish_nav_status(const std::string& twin_uuid, const std::string& action_id, const std::string& status,
                            std::optional<std::string> message = std::nullopt,
                            std::optional<double> progress = std::nullopt);

    /** @brief Publish health data for a twin served by this node. */
    void publish_health(const std::string& twin_uuid, const HealthStatus& health_data);

    /** @brief Publish an arbitrary event payload for a twin. */
    void publish_event(const std::string& twin_uuid, const std::string& event_type,
                       const std::map<std::string, std::string>& data);

    /** @brief Publish raw GPS data for a twin.  Stored as a ``twin_gps_update`` event. */
    void publish_gps(const std::string& twin_uuid, const GpsFix& fix);

    /** @brief Publish arbitrary telemetry data for a twin. */
    void publish_telemetry(const std::string& twin_uuid, const std::string& telemetry_type,
                           const std::map<std::string, std::string>& data);

    // --- Subscribe helpers (build topic and call mqtt_client()->subscribe) ---
    /** @brief Command callback signature used by edge-node MQTT subscriptions. */
    using CommandHandler = std::function<void(const std::string& json_payload)>;

    /** @brief Subscribe to navigation commands for a twin. */
    void subscribe_navigate_command(const std::string& twin_uuid, CommandHandler handler);

    /** @brief Subscribe to motion commands for a twin. */
    void subscribe_motion_command(const std::string& twin_uuid, CommandHandler handler);

    /** @brief Subscribe to mission commands for a twin. */
    void subscribe_mission_command(const std::string& twin_uuid, CommandHandler handler);

protected:
    /** Main loop; default: sleep 1s until !running_. Override for custom loop. */
    virtual void main_loop();

    /** Implement: node-specific setup after client is set. */
    virtual void setup() = 0;

    /** Implement: subscribe to MQTT command topics. */
    virtual void subscribe_to_commands() = 0;

    /** Implement: cleanup on shutdown. */
    virtual void cleanup() = 0;

    /** Implement: return health key-value map. */
    virtual HealthStatus build_health_status() = 0;

    EdgeNodeConfig config_;
    std::reference_wrapper<Client> client_;
    std::atomic<bool> running_{false};
    std::vector<std::string> discovered_twin_uuids_;
    std::chrono::steady_clock::time_point start_time_;
    std::thread health_thread_;
    std::shared_ptr<std::atomic<bool>> callback_guard_;

    /** @brief Background loop that periodically publishes health updates. */
    void health_loop();

    /** @brief Return the topic prefix for an MQTT client, or an empty prefix when none is attached. */
    static std::string topic_prefix_with(const std::shared_ptr<IMqttClient>& mqtt);

    /** @brief Return the node uptime in seconds since `run()` started. */
    double uptime_seconds() const;
};

} // namespace cyberwave

#endif // CYBERWAVE_EDGE_BASE_EDGE_NODE_H
