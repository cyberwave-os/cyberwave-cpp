/**
 * @brief Optional MQTT interface used by the SDK for real-time operations.
 *
 * Implement this interface and pass the instance to `Client::set_mqtt_client()`
 * to enable MQTT-backed joint updates, streaming, and twin commands.
 */

#ifndef CYBERWAVE_MQTT_INTERFACE_H
#define CYBERWAVE_MQTT_INTERFACE_H

#include <charconv>
#include <chrono>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cyberwave
{

/**
 * @brief The payload limit Cyberwave's brokers are configured with.
 *
 * Set on the brokers from CYB-4375 on (mosquitto `message_size_limit`, nanomq
 * `max_packet_size`); before it they accept up to the protocol's 256 MiB, so
 * this refuses payloads they would still deliver.
 *
 * A broker drops anything larger, and over MQTT 3.1.1 it cannot say so: a QoS 0
 * message just vanishes and a QoS 1 PUBACK reports success. The MQTT client
 * refuses such a payload before sending it, the only place the driver can hear
 * about it. A broker configured to accept more needs
 * `CYBERWAVE_MQTT_MAX_PAYLOAD_BYTES` raised to match, or 0. The same default as
 * the Python SDK's `DEFAULT_MAX_PAYLOAD_BYTES`.
 */
inline constexpr std::size_t DEFAULT_MQTT_MAX_PAYLOAD_BYTES = std::size_t{32} * 1024 * 1024;

/** @brief Overrides DEFAULT_MQTT_MAX_PAYLOAD_BYTES; 0 turns the check off. */
inline constexpr const char* MQTT_MAX_PAYLOAD_BYTES_ENV = "CYBERWAVE_MQTT_MAX_PAYLOAD_BYTES";

/**
 * @brief A `CYBERWAVE_MQTT_MAX_PAYLOAD_BYTES` value as a byte count.
 *
 * @return the count, where 0 turns the check off, or std::nullopt unless `raw`
 *         is a plain non-negative decimal integer (`32MiB`, `-1` and `1.5` are not).
 */
inline std::optional<std::size_t> parse_mqtt_max_payload_bytes(std::string_view raw)
{
    const auto first = raw.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return std::nullopt;
    raw = raw.substr(first, raw.find_last_not_of(" \t\r\n") - first + 1);
    std::size_t value = 0;
    const char* end = raw.data() + raw.size();
    const auto [parsed_to, error] = std::from_chars(raw.data(), end, value);
    if (error != std::errc{} || parsed_to != end)
        return std::nullopt;
    return value;
}

/** @brief The largest payload `mosquitto_publish` takes: it is handed the length as an `int`. */
inline constexpr std::size_t MQTT_MAX_PUBLISHABLE_PAYLOAD_BYTES =
    static_cast<std::size_t>(std::numeric_limits<int>::max());

/**
 * @brief The limit a payload is held to: `max_payload_bytes`, where 0 means no limit of our own,
 * but never more than mosquitto can publish.
 */
inline constexpr std::size_t mqtt_effective_max_payload_bytes(std::size_t max_payload_bytes)
{
    return max_payload_bytes == 0 || max_payload_bytes > MQTT_MAX_PUBLISHABLE_PAYLOAD_BYTES
               ? MQTT_MAX_PUBLISHABLE_PAYLOAD_BYTES
               : max_payload_bytes;
}

/** @brief Whether a `payload_bytes` payload is within `max_payload_bytes` (see mqtt_effective_max_payload_bytes). */
inline constexpr bool mqtt_payload_fits(std::size_t payload_bytes, std::size_t max_payload_bytes)
{
    return payload_bytes <= mqtt_effective_max_payload_bytes(max_payload_bytes);
}

/**
 * @brief The largest point cloud Cyberwave's telemetry pipeline records.
 *
 * Vector's `size_guard_colored_pointcloud` drops a larger `.../pointcloud`
 * payload before parsing it, since parsing is what bounds Vector's memory. The
 * broker still delivers it to live subscribers, so the client publishes it and
 * warns that it was not recorded. The same value as the Python SDK's
 * `POINTCLOUD_INGEST_MAX_PAYLOAD_BYTES`.
 */
inline constexpr std::size_t MQTT_POINTCLOUD_RECORDED_MAX_PAYLOAD_BYTES = std::size_t{8} * 1024 * 1024;

/** @brief Whether the cloud would not record a `payload_bytes` publish to `topic`, though the broker delivers it. */
inline bool mqtt_publish_unrecorded(std::string_view topic, std::size_t payload_bytes)
{
    constexpr std::string_view pointcloud_suffix = "/pointcloud";
    return payload_bytes > MQTT_POINTCLOUD_RECORDED_MAX_PAYLOAD_BYTES && topic.size() >= pointcloud_suffix.size() &&
           topic.substr(topic.size() - pointcloud_suffix.size()) == pointcloud_suffix;
}

namespace detail
{
/**
 * @brief Lets one report per key through per interval, and counts the rest.
 *
 * Not synchronised: the caller holds a lock around admit(). Remembers at
 * most `max_keys` keys, forgetting the least recently reported one first, so a
 * driver publishing to per-request topics cannot grow it without end; a
 * forgotten key at worst lets one extra report through.
 */
class ReportThrottle
{
public:
    static constexpr std::size_t kDefaultMaxKeys = 256;

    explicit ReportThrottle(std::chrono::steady_clock::duration interval, std::size_t max_keys = kDefaultMaxKeys)
        : interval_(interval), max_keys_(max_keys == 0 ? 1 : max_keys)
    {
    }

    std::size_t size() const { return entries_.size(); }

    /**
     * @return std::nullopt to stay quiet, else how many reports of `key` were
     *         held back since the last one let through.
     */
    std::optional<std::size_t> admit(const std::string& key, std::chrono::steady_clock::time_point now)
    {
        if (entries_.size() >= max_keys_ && entries_.find(key) == entries_.end())
        {
            // A linear scan, but only when a new key arrives at a full map.
            auto oldest = entries_.begin();
            for (auto candidate = entries_.begin(); candidate != entries_.end(); ++candidate)
            {
                if (candidate->second.reported_at < oldest->second.reported_at)
                    oldest = candidate;
            }
            entries_.erase(oldest);
        }
        auto [it, first] = entries_.try_emplace(key);
        Entry& entry = it->second;
        if (!first && now - entry.reported_at < interval_)
        {
            ++entry.suppressed;
            return std::nullopt;
        }
        entry.reported_at = now;
        return std::exchange(entry.suppressed, 0);
    }

private:
    struct Entry
    {
        std::chrono::steady_clock::time_point reported_at{};
        std::size_t suppressed = 0;
    };
    std::chrono::steady_clock::duration interval_;
    std::size_t max_keys_;
    std::map<std::string, Entry> entries_;
};
} // namespace detail

/** @brief Raw GNSS fix data for GPS telemetry publishing. */
struct GpsFix
{
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    std::optional<int> satellite_count;
    std::optional<int> signal_level;
    std::optional<double> compass_heading;
    std::optional<double> horizontal_accuracy;
    std::optional<double> vertical_accuracy;
    std::optional<std::string> fix_type;
};

/** @brief Callback type for subscribed MQTT JSON payloads. */
using MqttMessageHandler = std::function<void(const std::string& json_payload)>;

/** @brief RAII handle for a scoped MQTT subscription. */
struct MqttSubscriptionHandle
{
    virtual ~MqttSubscriptionHandle() = default;
};

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
     * @brief Subscribe to a topic and return a scoped handle that keeps the subscription active.
     *
     * Implementations that support explicit unsubscription should override this. The default
     * implementation falls back to `subscribe()` and returns `nullptr`, preserving existing behavior.
     */
    virtual std::unique_ptr<MqttSubscriptionHandle> subscribe_scoped(const std::string& topic,
                                                                     MqttMessageHandler handler)
    {
        subscribe(topic, std::move(handler));
        return nullptr;
    }

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
     *
     * ``velocity`` / ``effort`` use ``std::optional<double>``. ``std::nullopt``
     * means "channel not commanded/measured" and the field MUST be omitted on
     * the wire. An explicit ``0.0`` is a real command and is preserved.
     */
    virtual void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, double position_rad,
                                    std::optional<double> /*velocity*/, std::optional<double> /*effort*/,
                                    double /*timestamp*/ = -1.0, const std::string& /*source_type*/ = "")
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
     * Subscribe to WebRTC signaling with RAII unsubscribe when the handle is destroyed.
     * Prefer this over subscribe_webrtc_messages() for stream lifecycles tied to start()/stop().
     */
    virtual std::unique_ptr<MqttSubscriptionHandle> subscribe_webrtc_messages_scoped(const std::string& twin_uuid,
                                                                                     MqttMessageHandler handler)
    {
        struct WebRtcMessageSubscriptions final : MqttSubscriptionHandle
        {
            std::vector<std::unique_ptr<MqttSubscriptionHandle>> subscriptions;
        };

        auto handle = std::make_unique<WebRtcMessageSubscriptions>();
        const std::string base = get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/webrtc-";
        handle->subscriptions.push_back(subscribe_scoped(base + "offer", handler));
        handle->subscriptions.push_back(subscribe_scoped(base + "answer", handler));
        handle->subscriptions.push_back(subscribe_scoped(base + "candidate", std::move(handler)));
        return handle;
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
