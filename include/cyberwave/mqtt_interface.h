/**
 * @brief Optional MQTT interface used by the SDK for real-time operations.
 *
 * Implement this interface and pass the instance to `Client::set_mqtt_client()`
 * to enable MQTT-backed joint updates, streaming, and twin commands.
 */

#ifndef CYBERWAVE_MQTT_INTERFACE_H
#define CYBERWAVE_MQTT_INTERFACE_H

#include <functional>
#include <map>
#include <sstream>
#include <string>

namespace cyberwave
{

/** @brief Callback type for subscribed MQTT JSON payloads. */
using MqttMessageHandler = std::function<void(const std::string& json_payload)>;

/**
 * @brief Interface for SDK MQTT integrations.
 *
 * High-level helpers in this type provide default implementations built on top
 * of `publish()` and `subscribe()`, so implementers only need to supply the
 * core transport and can override specific helpers when desired.
 */
struct IMqttClient
{
    /** @brief Virtual destructor. */
    virtual ~IMqttClient() = default;

    /** @brief Return whether the broker connection is active. */
    virtual bool is_connected() const = 0;

    /** @brief Return the topic prefix used to construct full topic names. */
    virtual std::string get_topic_prefix() const = 0;

    /** @brief Publish a single joint position update in radians. */
    virtual void update_joint_state(const std::string& twin_uuid, const std::string& joint_name,
                                    double position_rad) = 0;

    /** @brief Publish a JSON payload to a fully qualified MQTT topic. */
    virtual void publish(const std::string& topic, const std::string& json_payload) = 0;

    /** @brief Subscribe to a topic and invoke `handler` for each JSON payload. */
    virtual void subscribe(const std::string& topic, MqttMessageHandler handler) = 0;

    /**
     * Connect to the MQTT broker. Default no-op (implementors override if needed).
     * Mirrors Python MqttClient.connect().
     */
    virtual void connect() {}

    /**
     * Disconnect from the MQTT broker. Default no-op (implementors override if needed).
     * Mirrors Python MqttClient.disconnect().
     */
    virtual void disconnect() {}

    /**
     * Rich single-joint update: position + optional velocity, effort, timestamp, source_type.
     * Default delegates to the pure-virtual 3-arg update_joint_state().
     * Mirrors Python MqttClient.update_joint_state() (single joint).
     */
    virtual void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, double position_rad,
                                    double /*velocity*/, double /*effort*/, double /*timestamp*/ = -1.0,
                                    const std::string& /*source_type*/ = "")
    {
        update_joint_state(twin_uuid, joint_name, position_rad);
    }

    // --- Typed publish helpers (default implementations delegate to publish()) ---

    /**
     * @brief Publish a twin position update.
     * @param twin_uuid Twin UUID.
     * @param x X position in meters.
     * @param y Y position in meters.
     * @param z Z position in meters.
     */
    virtual void update_twin_position(const std::string& twin_uuid, double x, double y, double z)
    {
        std::ostringstream out;
        out << "{\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z << "}";
        publish(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/position", out.str());
    }

    /**
     * @brief Publish a twin rotation update as a quaternion.
     * @param twin_uuid Twin UUID.
     * @param w Quaternion W component.
     * @param x Quaternion X component.
     * @param y Quaternion Y component.
     * @param z Quaternion Z component.
     */
    virtual void update_twin_rotation(const std::string& twin_uuid, double w, double x, double y, double z)
    {
        std::ostringstream out;
        out << "{\"w\":" << w << ",\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z << "}";
        publish(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/rotation", out.str());
    }

    /**
     * @brief Publish a twin scale update.
     * @param twin_uuid Twin UUID.
     * @param x Scale on the X axis.
     * @param y Scale on the Y axis.
     * @param z Scale on the Z axis.
     */
    virtual void update_twin_scale(const std::string& twin_uuid, double x, double y, double z)
    {
        std::ostringstream out;
        out << "{\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z << "}";
        publish(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/scale", out.str());
    }

    /**
     * Publish joint states to cyberwave/twin/{uuid}/joint_states.
     * Extended version with velocities, efforts, timestamp, workload_uuid, session_id.
     * Mirrors Python MqttClient.update_joints_state().
     */
    virtual void update_joints_state(const std::string& twin_uuid, const std::map<std::string, double>& positions,
                                     const std::string& source_type = "",
                                     const std::map<std::string, double>& velocities = {},
                                     const std::map<std::string, double>& efforts = {}, double timestamp = -1.0,
                                     const std::string& workload_uuid = "", const std::string& session_id = "",
                                     const std::string& source_subtype = "")
    {
        bool use_aggregated = !velocities.empty() || !efforts.empty() || timestamp >= 0.0 || !workload_uuid.empty() ||
                              !session_id.empty() || !source_subtype.empty();
        std::ostringstream out;
        if (use_aggregated)
        {
            out << "{";
            if (!source_type.empty())
                out << "\"source_type\":\"" << source_type << "\",";
            if (!source_subtype.empty())
                out << "\"source_subtype\":\"" << source_subtype << "\",";
            if (timestamp >= 0.0)
                out << "\"timestamp\":" << timestamp << ",";
            if (!workload_uuid.empty())
                out << "\"workload_uuid\":\"" << workload_uuid << "\",";
            if (!session_id.empty())
                out << "\"session_id\":\"" << session_id << "\",";
            auto write_map = [&](const std::string& key, const std::map<std::string, double>& m)
            {
                out << "\"" << key << "\":{";
                bool first = true;
                for (const auto& kv : m)
                {
                    if (!first)
                        out << ",";
                    out << "\"" << kv.first << "\":" << kv.second;
                    first = false;
                }
                out << "}";
            };
            write_map("positions", positions);
            if (!velocities.empty())
            {
                out << ",";
                write_map("velocities", velocities);
            }
            if (!efforts.empty())
            {
                out << ",";
                write_map("efforts", efforts);
            }
            out << "}";
        }
        else
        {
            out << "{";
            if (!source_type.empty())
                out << "\"source_type\":\"" << source_type << "\",";
            out << "\"positions\":{";
            bool first = true;
            for (const auto& kv : positions)
            {
                if (!first)
                    out << ",";
                out << "\"" << kv.first << "\":" << kv.second;
                first = false;
            }
            out << "}}";
        }
        publish(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/joint_states", out.str());
    }

    // --- Typed subscribe helpers (default implementations delegate to subscribe()) ---

    /**
     * @brief Subscribe to all MQTT updates for a twin.
     * @param twin_uuid Twin UUID.
     * @param handler Callback invoked for each matching message.
     */
    virtual void subscribe_twin(const std::string& twin_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/#", std::move(handler));
    }

    /**
     * @brief Subscribe to twin position updates.
     * @param twin_uuid Twin UUID.
     * @param handler Callback invoked for each matching message.
     */
    virtual void subscribe_twin_position(const std::string& twin_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/position", std::move(handler));
    }

    /**
     * @brief Subscribe to twin rotation updates.
     * @param twin_uuid Twin UUID.
     * @param handler Callback invoked for each matching message.
     */
    virtual void subscribe_twin_rotation(const std::string& twin_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/rotation", std::move(handler));
    }

    /**
     * @brief Subscribe to twin joint-state updates.
     * @param twin_uuid Twin UUID.
     * @param handler Callback invoked for each matching message.
     */
    virtual void subscribe_twin_joint_states(const std::string& twin_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/joint_states", std::move(handler));
    }

    // --- Telemetry helpers ---

    /**
     * Publish a telemetry_start message for the given twin.
     * Topic: {prefix}cyberwave/twin/{uuid}/telemetry
     * Mirrors Python MqttClient.publish_telemetry_start().
     */
    virtual void publish_telemetry_start(const std::string& twin_uuid, const std::string& metadata_json = "")
    {
        std::ostringstream out;
        out << "{\"type\":\"telemetry_start\"";
        if (!metadata_json.empty() && metadata_json != "{}")
            out << ",\"metadata\":" << metadata_json;
        out << "}";
        publish(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/telemetry", out.str());
    }

    /**
     * Publish initial observations for the given twin.
     * Topic: {prefix}cyberwave/twin/{uuid}/telemetry
     * Mirrors Python MqttClient.publish_initial_observation().
     */
    virtual void publish_initial_observation(const std::string& twin_uuid, const std::string& observations_json,
                                             double fps = 30.0)
    {
        std::ostringstream out;
        out << "{\"type\":\"initial_observation\",\"fps\":" << fps
            << ",\"observations\":" << (observations_json.empty() ? "{}" : observations_json) << "}";
        publish(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/telemetry", out.str());
    }

    // --- Environment pub/sub ---

    /**
     * Subscribe to all updates for an environment.
     * Topic: {prefix}cyberwave/environment/{uuid}/+
     * Mirrors Python MqttClient.subscribe_environment().
     */
    virtual void subscribe_environment(const std::string& environment_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/environment/" + environment_uuid + "/+", std::move(handler));
    }

    /**
     * Publish an environment update.
     * Topic: {prefix}cyberwave/environment/{uuid}/{update_type}
     * Mirrors Python MqttClient.publish_environment_update().
     */
    virtual void publish_environment_update(const std::string& environment_uuid, const std::string& update_type,
                                            const std::string& data_json)
    {
        publish(get_topic_prefix() + "cyberwave/environment/" + environment_uuid + "/" + update_type,
                data_json.empty() ? "{}" : data_json);
    }

    // --- Stream subscribe helpers ---

    /**
     * Subscribe to video stream for a twin.
     * Topic: {prefix}cyberwave/twin/{uuid}/video
     * Mirrors Python MqttClient.subscribe_video_stream().
     */
    virtual void subscribe_video_stream(const std::string& twin_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/video", std::move(handler));
    }

    /**
     * Subscribe to depth stream for a twin.
     * Topic: {prefix}cyberwave/twin/{uuid}/depth
     * Mirrors Python MqttClient.subscribe_depth_stream().
     */
    virtual void subscribe_depth_stream(const std::string& twin_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/depth", std::move(handler));
    }

    /**
     * Subscribe to point cloud stream for a twin.
     * Topic: {prefix}cyberwave/twin/{uuid}/pointcloud
     * Mirrors Python MqttClient.subscribe_pointcloud_stream().
     */
    virtual void subscribe_pointcloud_stream(const std::string& twin_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/pointcloud", std::move(handler));
    }

    // --- WebRTC + command message pub/sub ---

    /**
     * Publish a WebRTC signaling message for a twin.
     * Topic: {prefix}cyberwave/twin/{uuid}/webrtc
     * Mirrors Python MqttClient.publish_webrtc_message().
     */
    virtual void publish_webrtc_message(const std::string& twin_uuid, const std::string& data_json)
    {
        // Keep default behavior symmetric with Python SDK:
        // route by signaling type to topic-specific channels.
        const std::string payload = data_json.empty() ? "{}" : data_json;
        std::string topic = get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/webrtc";

        // Minimal JSON type detection without bringing extra deps into interface header.
        if (payload.find("\"type\"") != std::string::npos)
        {
            if (payload.find("\"offer\"") != std::string::npos)
            {
                topic = get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/webrtc-offer";
            }
            else if (payload.find("\"answer\"") != std::string::npos)
            {
                topic = get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/webrtc-answer";
            }
            else if (payload.find("\"candidate\"") != std::string::npos)
            {
                topic = get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/webrtc-candidate";
            }
        }
        publish(topic, payload);
    }

    /**
     * Subscribe to WebRTC signaling messages for a twin.
     * Topic: {prefix}cyberwave/twin/{uuid}/webrtc
     * Mirrors Python MqttClient.subscribe_webrtc_messages().
     */
    virtual void subscribe_webrtc_messages(const std::string& twin_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/webrtc-offer", handler);
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/webrtc-answer", handler);
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/webrtc-candidate", std::move(handler));
    }

    /**
     * Publish an edge command response message for a twin.
     * Topic: {prefix}cyberwave/twin/{uuid}/command
     * Mirrors Python MqttClient.publish_command_message().
     */
    virtual void publish_command_message(const std::string& twin_uuid, const std::string& status_json)
    {
        publish(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/command",
                status_json.empty() ? "{}" : status_json);
    }

    /**
     * Subscribe to command messages for a twin.
     * Topic: {prefix}cyberwave/twin/{uuid}/command
     * Mirrors Python MqttClient.subscribe_command_message().
     */
    virtual void subscribe_command_message(const std::string& twin_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/command", std::move(handler));
    }

    // --- Ping / pong ---

    /**
     * Send a ping message to a resource.
     * Topic: {prefix}cyberwave/ping/{uuid}
     * Mirrors Python MqttClient.ping().
     */
    virtual void ping(const std::string& resource_uuid)
    {
        publish(get_topic_prefix() + "cyberwave/ping/" + resource_uuid, "{}");
    }

    /**
     * Subscribe to pong responses for a resource.
     * Topic: {prefix}cyberwave/pong/{uuid}
     * Mirrors Python MqttClient.subscribe_pong().
     */
    virtual void subscribe_pong(const std::string& resource_uuid, MqttMessageHandler handler)
    {
        subscribe(get_topic_prefix() + "cyberwave/pong/" + resource_uuid, std::move(handler));
    }
};

} // namespace cyberwave

#endif // CYBERWAVE_MQTT_INTERFACE_H
