/**
 * @brief AMR edge-node base class built on top of `BaseEdgeNode`.
 */

#ifndef CYBERWAVE_EDGE_AMR_EDGE_NODE_H
#define CYBERWAVE_EDGE_AMR_EDGE_NODE_H

#include "cyberwave/edge/amr_adapter.h"
#include "cyberwave/edge/amr_types.h"
#include "cyberwave/edge/base_edge_node.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace cyberwave
{

/**
 * @brief Base AMR/AGV edge node that coordinates adapter I/O and Cyberwave topics.
 */
class AMREdgeNode : public BaseEdgeNode
{
public:
    /** Optional adapter config; defaults to `AdapterConfig::from_env()` when unset. */
    explicit AMREdgeNode(const EdgeNodeConfig& config, Client& client,
                         std::optional<AdapterConfig> adapter_config = std::nullopt);

    /** @brief Virtual destructor. */
    ~AMREdgeNode() override;

    AMREdgeNode(const AMREdgeNode&) = delete;
    AMREdgeNode& operator=(const AMREdgeNode&) = delete;

    /** @brief Return the resolved adapter configuration. */
    const AdapterConfig& adapter_config() const noexcept { return adapter_config_; }

    /** @brief Return the mutable AMR adapter instance, if created. */
    std::shared_ptr<IAMRAdapter> adapter() noexcept { return adapter_; }

    /** @brief Return the read-only AMR adapter instance, if created. */
    std::shared_ptr<const IAMRAdapter> adapter() const noexcept { return adapter_; }

protected:
    /** @brief Perform node-specific setup and adapter creation. */
    void setup() override;

    /** @brief Subscribe the node to incoming command topics. */
    void subscribe_to_commands() override;

    /** @brief Stop threads and disconnect the adapter. */
    void cleanup() override;

    /** @brief Build the health payload published for this node. */
    HealthStatus build_health_status() override;

    /** Create protocol-specific adapter. Subclass must implement. */
    virtual std::shared_ptr<IAMRAdapter> create_adapter() = 0;

    /** Connect adapter (default: single try; override for retry). */
    virtual void connect_adapter();

    AdapterConfig adapter_config_;
    std::shared_ptr<IAMRAdapter> adapter_;
    std::optional<RobotTelemetry> current_telemetry_;
    mutable std::mutex telemetry_mutex_;
    std::map<std::string, std::map<std::string, std::string>> active_actions_;
    std::mutex active_actions_mutex_;
    std::thread position_thread_;
    std::thread telemetry_thread_;

    /** @brief Background loop that publishes robot position updates. */
    void position_loop();

    /** @brief Background loop that polls and publishes robot telemetry. */
    void telemetry_loop();

    /** @brief Handle adapter status callbacks and forward them to Cyberwave topics. */
    void on_adapter_status(const std::string& action_id, const std::string& status, std::optional<std::string> message,
                           std::optional<double> progress);

    /** @brief Handle a navigation command payload for a specific twin. */
    void handle_navigate_command(const std::string& twin_uuid, const std::string& json_payload);

    /** @brief Handle a mission command payload for a specific twin. */
    void handle_mission_command(const std::string& twin_uuid, const std::string& json_payload);
};

} // namespace cyberwave

#endif // CYBERWAVE_EDGE_AMR_EDGE_NODE_H
