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
#include <string>

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
     * @brief Return a twin handle for a UUID or slug-like identifier.
     * @param slug Twin UUID or slug.
     * @return Twin handle with lightweight identity fields populated.
     */
    Twin twin(const std::string& slug) const;

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
     * Set the default source_type for all subsequent commands.
     * Accepts aliases: "simulation"/"sim" → "sim", "real-world"/"real"/"tele"/"teleoperation" → "tele".
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
     * @brief Return the attached MQTT client, if any.
     * @return Attached MQTT client or `nullptr` when none is configured.
     */
    std::shared_ptr<IMqttClient> mqtt_client() const noexcept { return mqtt_; }

private:
    friend struct ClientAccess;

    Config config_;
    struct RestState;
    std::unique_ptr<RestState> rest_;
    std::shared_ptr<IMqttClient> mqtt_;
};

} // namespace cyberwave

#endif // CYBERWAVE_CLIENT_H
