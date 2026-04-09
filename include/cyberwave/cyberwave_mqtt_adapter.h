/**
 * @brief Header-only adapter bridging the concrete MQTT client to `IMqttClient`.
 *
 * Include this header when you want the SDK's abstract MQTT interface backed by
 * `CyberwaveMQTTClient` from the `mqtt/` implementation (libmosquitto).
 */

#pragma once

#include "cyberwave/config.h"
#include "cyberwave/mqtt_interface.h"
#include "mqtt_client.h" // CyberwaveMQTTClient — from mqtt/ directory

#include <memory>
#include <string>

namespace cyberwave
{

/**
 * @brief Adapter from `CyberwaveMQTTClient` to the SDK's `IMqttClient` interface.
 */
class CyberwaveMqttAdapter : public IMqttClient
{
public:
    class ScopedSubscriptionHandle : public MqttSubscriptionHandle
    {
    public:
        ScopedSubscriptionHandle(std::weak_ptr<CyberwaveMQTTClient> client, SubscriptionId subscription_id)
            : client_(std::move(client)), subscription_id_(subscription_id)
        {
        }

        ~ScopedSubscriptionHandle() override
        {
            if (auto client = client_.lock())
            {
                client->unsubscribe(subscription_id_);
            }
        }

    private:
        std::weak_ptr<CyberwaveMQTTClient> client_;
        SubscriptionId subscription_id_;
    };

    /**
     * Construct from a cyberwave::Config (loaded from environment or set explicitly).
     * Does NOT connect automatically — call connect() when ready.
     */
    explicit CyberwaveMqttAdapter(const Config& cfg)
    {
        CyberwaveConfig mosq_cfg;
        mosq_cfg.mqtt_host = cfg.mqtt_host;
        mosq_cfg.mqtt_port = static_cast<int>(cfg.mqtt_port);
        mosq_cfg.mqtt_username = cfg.mqtt_username;
        mosq_cfg.mqtt_password = cfg.mqtt_password;
        mosq_cfg.mqtt_api_token = cfg.mqtt_password.empty() ? cfg.api_key : "";
        mosq_cfg.mqtt_use_tls = cfg.mqtt_use_tls;
        mosq_cfg.mqtt_tls_ca_cert = cfg.mqtt_tls_ca_cert;
        mosq_cfg.topic_prefix = cfg.topic_prefix;
        mosq_cfg.source_type = cfg.source_type;
        inner_ = std::make_shared<CyberwaveMQTTClient>(mosq_cfg);
    }

    // -----------------------------------------------------------------------
    // IMqttClient — pure virtual implementations
    // -----------------------------------------------------------------------

    bool is_connected() const override { return inner_->is_connected(); }

    std::string get_topic_prefix() const override { return inner_->get_topic_prefix(); }

    void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, double position_rad) override
    {
        JointState js;
        js.position = position_rad;
        inner_->update_joint_state(twin_uuid, joint_name, js);
    }

    void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, double position_rad,
                            double velocity, double effort, double timestamp = -1.0,
                            const std::string& source_type = "") override
    {
        JointState js;
        js.position = position_rad;
        js.velocity = velocity;
        js.effort = effort;
        inner_->update_joint_state(twin_uuid, joint_name, js, timestamp, source_type);
    }

    void publish(const std::string& topic, const std::string& json_payload) override
    {
        inner_->publish(topic, json_payload);
    }

    void subscribe(const std::string& topic, MqttMessageHandler handler) override
    {
        inner_->subscribe(topic,
                          [handler](const std::string& /*topic*/, const nlohmann::json& msg) { handler(msg.dump()); });
    }

    std::unique_ptr<MqttSubscriptionHandle> subscribe_scoped(const std::string& topic,
                                                             MqttMessageHandler handler) override
    {
        const auto subscription_id = inner_->subscribe_with_id(
            topic, [handler](const std::string& /*topic*/, const nlohmann::json& msg) { handler(msg.dump()); });
        return std::make_unique<ScopedSubscriptionHandle>(inner_, subscription_id);
    }

    // -----------------------------------------------------------------------
    // IMqttClient — lifecycle
    // -----------------------------------------------------------------------

    void connect() override { inner_->connect(); }

    void disconnect() override { inner_->disconnect(); }

    void update_joints_state(const std::string& twin_uuid, const std::map<std::string, double>& positions,
                             const std::string& source_type = "", const std::map<std::string, double>& velocities = {},
                             const std::map<std::string, double>& efforts = {}, double timestamp = -1.0,
                             const std::string& workload_uuid = "", const std::string& session_id = "",
                             const std::string& source_subtype = "") override
    {
        inner_->update_joints_state(twin_uuid, positions, source_type, velocities, efforts, timestamp, workload_uuid,
                                    session_id, source_subtype);
    }

    // -----------------------------------------------------------------------
    // IMqttClient — high-level subscriptions
    // -----------------------------------------------------------------------

    void subscribe_twin(const std::string& twin_uuid, MqttMessageHandler handler) override
    {
        inner_->subscribe_twin(twin_uuid, wrap(handler));
    }

    void subscribe_twin_position(const std::string& twin_uuid, MqttMessageHandler handler) override
    {
        inner_->subscribe_twin_position(twin_uuid, wrap(handler));
    }

    void subscribe_twin_rotation(const std::string& twin_uuid, MqttMessageHandler handler) override
    {
        inner_->subscribe_twin_rotation(twin_uuid, wrap(handler));
    }

    void subscribe_twin_joint_states(const std::string& twin_uuid, MqttMessageHandler handler) override
    {
        inner_->subscribe_joint_states(twin_uuid, wrap(handler));
    }

    void subscribe_command_message(const std::string& twin_uuid, MqttMessageHandler handler) override
    {
        inner_->subscribe_command_message(twin_uuid, wrap(handler));
    }

    void publish_command_message(const std::string& twin_uuid, const std::string& status_json) override
    {
        inner_->publish_command_message(twin_uuid, status_json);
    }

    void subscribe_video_stream(const std::string& twin_uuid, MqttMessageHandler handler) override
    {
        inner_->subscribe_video_stream(twin_uuid, wrap(handler));
    }

    void subscribe_depth_stream(const std::string& twin_uuid, MqttMessageHandler handler) override
    {
        inner_->subscribe_depth_stream(twin_uuid, wrap(handler));
    }

    void subscribe_pointcloud_stream(const std::string& twin_uuid, MqttMessageHandler handler) override
    {
        inner_->subscribe_pointcloud_stream(twin_uuid, wrap(handler));
    }

    void subscribe_environment(const std::string& env_uuid, MqttMessageHandler handler) override
    {
        inner_->subscribe_environment(env_uuid, wrap(handler));
    }

    void publish_telemetry_start(const std::string& twin_uuid, const std::string& metadata_json = "") override
    {
        nlohmann::json meta = metadata_json.empty() ? nlohmann::json::object() : nlohmann::json::parse(metadata_json);
        inner_->publish_telemetry_start(twin_uuid, meta);
    }

    void publish_initial_observation(const std::string& twin_uuid, const std::string& observations_json,
                                     double fps = 30.0) override
    {
        nlohmann::json obs =
            observations_json.empty() ? nlohmann::json::object() : nlohmann::json::parse(observations_json);
        inner_->publish_initial_observation(twin_uuid, obs, fps);
    }

    // -----------------------------------------------------------------------
    // Access to the underlying client (advanced use)
    // -----------------------------------------------------------------------

    CyberwaveMQTTClient& inner() noexcept { return *inner_; }
    const CyberwaveMQTTClient& inner() const noexcept { return *inner_; }

private:
    std::shared_ptr<CyberwaveMQTTClient> inner_;

    static MessageCallback wrap(MqttMessageHandler handler) // NOLINT
    {
        return [h = std::move(handler)](const std::string& /*topic*/, const nlohmann::json& msg) { h(msg.dump()); };
    }
};

// Backwards-compatible alias for code that still references the old name.
using PahoMqttAdapter = CyberwaveMqttAdapter;

} // namespace cyberwave
