/**
 * @brief Main client entry point for the Cyberwave C++ SDK.
 *
 * Mirrors the high-level Python client. The client owns REST state and can
 * optionally be paired with an `IMqttClient` implementation for real-time
 * publish/subscribe workflows.
 */

#ifndef CYBERWAVE_CLIENT_H
#define CYBERWAVE_CLIENT_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cyberwave/config.h"
#include "cyberwave/mqtt_interface.h"

namespace cyberwave
{

class Twin;
class TwinAlertManager;
class WorkspaceManager;
class EdgeManager;
class ProjectManager;
class EnvironmentManager;
class AssetManager;
class TwinManager;
class WorkflowManager;
class WorkflowRunManager;
class Scene;
class DataBus;
class DataBackend;
class HookRegistry;

struct TwinResolveOptions
{
    std::string twin_id;
    std::string environment_id;
    std::string name;
    std::string description;
    std::vector<double> position;
    std::vector<double> orientation;
    std::optional<bool> fixed_base;
    bool reuse_existing{true};
    bool create_if_missing{true};
};

/**
 * @brief Main SDK client for REST and optional MQTT operations.
 */
class Client
{
public:
    /**
     * @brief Construct a client from an immutable configuration object.
     * @param config SDK configuration including base URL, API key, and MQTT defaults.
     */
    explicit Client(const Config& config);

    /**
     * @brief Construct a client by moving a configuration object.
     * @param config SDK configuration including base URL, API key, and MQTT defaults.
     */
    explicit Client(Config&& config);

    /** @brief Destroy the client and release owned REST state. */
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(Client&&) = delete;

    /**
     * @brief Return the effective configuration used by this client.
     * @return Const reference to the configuration object.
     */
    const Config& config() const noexcept { return config_; }

    /**
     * @brief Return or create a twin handle from a twin UUID or asset registry identifier.
     * @param identifier Existing twin UUID, or an asset registry ID / alias to resolve.
     * @param options Optional lookup and creation controls.
     * @return Twin handle populated from backend state when REST is configured, otherwise a lightweight stub.
     *
     * When resolving an asset key and no environment is configured, this mirrors
     * Python quickstart behavior by creating and caching a default workspace /
     * project / environment context as needed.
     */
    Twin twin(const std::string& identifier, const TwinResolveOptions& options = {}) const;

    /** @brief Return the workspace manager. */
    WorkspaceManager workspaces() const;

    /** @brief Return the edge manager. */
    EdgeManager edges() const;

    /** @brief Return the project manager. */
    ProjectManager projects() const;

    /** @brief Return the environment manager. */
    EnvironmentManager environments() const;

    /** @brief Return the asset manager. */
    AssetManager assets() const;

    /** @brief Return the twin manager. */
    TwinManager twins() const;

    /** @brief Return the workflow manager. */
    WorkflowManager workflows() const;

    /** @brief Return the workflow run manager. */
    WorkflowRunManager workflow_runs() const;

    /**
     * @brief Return a data-plane facade bound to the configured twin UUID.
     * @param backend Transport backend implementation.
     * @param sensor_name Optional sensor qualifier for key generation.
     * @param key_prefix Key-expression prefix (default `cw`).
     * @return DataBus facade scoped to this client's `Config.twin_uuid`.
     */
    DataBus data(std::shared_ptr<DataBackend> backend, const std::string& sensor_name = "",
                 const std::string& key_prefix = "cw") const;

    /** @brief Return the local worker hook registry. */
    HookRegistry& hooks();
    const HookRegistry& hooks() const;

    /**
     * @brief Return a scene facade for a specific environment.
     * @param environment_id Environment UUID.
     * @return Scene helper bound to the given environment.
     */
    Scene get_scene(const std::string& environment_id) const;

    /**
     * @brief Disconnect and detach the attached MQTT client without invalidating REST access.
     *
     * Safe to call when no MQTT client is configured or connected.
     */
    void disconnect();

    /**
     * Set the default source_type for generic state/event publishing.
     * Accepts aliases: "simulation"/"sim" → runtime_mode="simulation", source_type="sim";
     * "live"/"real-world"/"real"/"tele"/"teleoperation" → runtime_mode="live", source_type="edge".
     * Throws std::invalid_argument for unknown modes.
     * Mirrors Python Cyberwave.affect().
     */
    Client& affect(const std::string& mode);

    /**
     * @brief Return the current default source type for outgoing commands.
     * @return Source type string such as `edge`, `tele`, or `sim`.
     */
    const std::string& source_type() const noexcept { return config_.source_type; }

    /**
     * @brief Attach an MQTT client implementation to this SDK client.
     * @param mqtt Shared MQTT client implementation.
     */
    void set_mqtt_client(std::shared_ptr<IMqttClient> mqtt);

    /**
     * Publish a business event for a twin over MQTT.
     * Mirrors Python Cyberwave.publish_event().
     */
    void publish_event(const std::string& twin_uuid, const std::string& event_type, const std::string& data_json = "{}",
                       const std::string& source = "edge_node") const;

    /**
     * @brief Return the attached MQTT client, if any.
     * @return Attached MQTT client or `nullptr` when none is configured.
     */
    std::shared_ptr<IMqttClient> mqtt_client() const noexcept { return mqtt_; }

private:
    friend struct ClientAccess;

    mutable Config config_;
    struct RestState;
    std::unique_ptr<RestState> rest_;
    std::shared_ptr<IMqttClient> mqtt_;
    std::unique_ptr<HookRegistry> hook_registry_;
    std::shared_ptr<void> lifetime_token_{std::make_shared<int>(0)};
};

} // namespace cyberwave

#endif // CYBERWAVE_CLIENT_H
