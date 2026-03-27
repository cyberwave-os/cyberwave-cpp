#pragma once

#include "cyberwave/mqtt_interface.h"
#include "mqtt_client.h"

#include <string>

namespace cyberwave
{

class CyberwaveMqttClientAdapter : public IMqttClient
{
public:
    explicit CyberwaveMqttClientAdapter(CyberwaveMQTTClient& client) : client_(client) {}

    bool is_connected() const override { return client_.is_connected(); }
    std::string get_topic_prefix() const override { return client_.get_topic_prefix(); }

    void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, double position_rad) override
    {
        JointState state;
        state.position = position_rad;
        client_.update_joint_state(twin_uuid, joint_name, state);
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

private:
    CyberwaveMQTTClient& client_;
};

} // namespace cyberwave
