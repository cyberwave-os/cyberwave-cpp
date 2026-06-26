#pragma once

#include "cyberwave/mqtt_interface.h"
#include "mqtt_client.h"

#include <memory>
#include <string>

namespace cyberwave
{

class CyberwaveMqttClientAdapter : public IMqttClient
{
public:
    class ScopedSubscriptionHandle : public MqttSubscriptionHandle
    {
    public:
        explicit ScopedSubscriptionHandle(std::shared_ptr<bool> active) : active_(std::move(active)) {}

        ~ScopedSubscriptionHandle() override
        {
            if (active_)
            {
                *active_ = false;
            }
        }

    private:
        std::shared_ptr<bool> active_;
    };

    explicit CyberwaveMqttClientAdapter(CyberwaveMQTTClient& client) : client_(client) {}

    bool is_connected() const override { return client_.is_connected(); }
    std::string get_topic_prefix() const override { return client_.get_topic_prefix(); }

    void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, double position_rad) override
    {
        JointState state;
        state.position = position_rad;
        client_.update_joint_state(twin_uuid, joint_name, state);
    }

    using IMqttClient::update_joint_state; // surface base overloads
    void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, double position_rad,
                            std::optional<double> velocity, std::optional<double> effort, double timestamp = -1.0,
                            const std::string& source_type = "") override
    {
        JointState state;
        state.position = position_rad;
        if (velocity.has_value())
            state.velocity = *velocity;
        if (effort.has_value())
            state.effort = *effort;
        const std::string st = source_type.empty() ? std::string{} : source_type;
        if (timestamp >= 0.0)
            client_.update_joint_state(twin_uuid, joint_name, state, timestamp, st);
        else
            client_.update_joint_state(twin_uuid, joint_name, state, st);
    }

    void publish(const std::string& topic, const std::string& json_payload) override
    {
        client_.publish(topic, json_payload, 0);
    }

    void subscribe(const std::string& topic, MqttMessageHandler handler) override
    {
        client_.subscribe(
            topic, [h = std::move(handler)](const std::string&, const nlohmann::json& message) { h(message.dump()); },
            0);
    }

    std::unique_ptr<MqttSubscriptionHandle> subscribe_scoped(const std::string& topic,
                                                             MqttMessageHandler handler) override
    {
        auto active = std::make_shared<bool>(true);
        client_.subscribe(
            topic,
            [active, h = std::move(handler)](const std::string&, const nlohmann::json& message)
            {
                if (!*active)
                {
                    return;
                }
                h(message.dump());
            },
            0);
        return std::make_unique<ScopedSubscriptionHandle>(std::move(active));
    }

private:
    CyberwaveMQTTClient& client_;
};

} // namespace cyberwave
