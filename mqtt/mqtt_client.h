#pragma once

#include "constants.h"
#include <cyberwave/mqtt_interface.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mosquitto.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace cyberwave
{

using json = nlohmann::json;
using SubscriptionId = std::uint64_t;

// Configuration structure
struct CyberwaveConfig
{
    std::string mqtt_host;
    int mqtt_port = 0;
    std::string mqtt_username;
    std::string mqtt_api_token;
    std::string topic_prefix;
    std::string source_type = SOURCE_TYPE_EDGE;
    // Deprecated alias for mqtt_api_token, kept for backwards compatibility.
    std::string mqtt_password;
    bool mqtt_use_tls = false;
    std::string mqtt_tls_ca_cert;
    int mqtt_protocol = 0;
    std::string runtime_mode = "live";
    std::string twin_uuid;
};

// Data structures
struct Position
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Rotation
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 1.0;
};

struct Scale
{
    double x = 1.0;
    double y = 1.0;
    double z = 1.0;
};

struct JointState
{
    std::optional<double> position;
    std::optional<double> velocity;
    std::optional<double> effort;
};

// RAII handle for struct mosquitto*
struct MosquittoDeleter
{
    void operator()(struct mosquitto* m) const noexcept;
};
using MosquittoPtr = std::unique_ptr<struct mosquitto, MosquittoDeleter>;

// Callback type for message handling
using MessageCallback = std::function<void(const std::string& topic, const json& message)>;

/**
 * MQTT client wrapper for real-time communication with Cyberwave platform.
 *
 * Uses libmosquitto for the underlying MQTT transport. The background I/O
 * thread (mosquitto_loop_start) drains the outbound queue to the socket,
 * providing natural TCP backpressure without bounded-buffer overflow errors.
 */
class CyberwaveMQTTClient
{
public:
    /**
     * Initialize MQTT client from a CyberwaveConfig object.
     *
     * @param config Cyberwave configuration object containing MQTT settings
     */
    explicit CyberwaveMQTTClient(const CyberwaveConfig& config);

    /**
     * Destructor - ensures proper disconnection
     */
    ~CyberwaveMQTTClient();

    // Disable copy
    CyberwaveMQTTClient(const CyberwaveMQTTClient&) = delete;
    CyberwaveMQTTClient& operator=(const CyberwaveMQTTClient&) = delete;

    /**
     * Connect to the MQTT broker.
     */
    void connect();

    /**
     * Disconnect from the MQTT broker.
     */
    void disconnect();

    /**
     * Check if the client is connected to the MQTT broker.
     *
     * @return true if connected, false otherwise
     */
    bool is_connected() const;

    /**
     * Python-parity alias for is_connected().
     */
    bool connected() const { return is_connected(); }

    /**
     * Get the topic prefix used by this MQTT client.
     *
     * @return topic prefix string
     */
    std::string get_topic_prefix() const;

    /**
     * Python-parity alias for get_topic_prefix().
     */
    std::string topic_prefix() const { return get_topic_prefix(); }

    // Twin-related methods

    /**
     * Subscribe to twin position updates.
     *
     * @param twin_uuid UUID of the twin to monitor
     * @param callback Function to call when position updates are received
     */
    void subscribe_twin_position(const std::string& twin_uuid, MessageCallback callback);

    /**
     * Subscribe to twin rotation updates.
     *
     * @param twin_uuid UUID of the twin to monitor
     * @param callback Function to call when rotation updates are received
     */
    void subscribe_twin_rotation(const std::string& twin_uuid, MessageCallback callback);

    /**
     * Subscribe to joint state updates.
     *
     * @param twin_uuid UUID of the twin to monitor
     * @param callback Function to call when joint state updates are received
     */
    void subscribe_joint_states(const std::string& twin_uuid, MessageCallback callback);

    /**
     * Subscribe to all twin updates via MQTT.
     *
     * Python parity: subscribe_twin(twin_uuid, on_update)
     */
    void subscribe_twin(const std::string& twin_uuid, MessageCallback callback);

    /**
     * Python-parity alias for subscribe_joint_states().
     */
    void subscribe_twin_joint_states(const std::string& twin_uuid, MessageCallback callback)
    {
        subscribe_joint_states(twin_uuid, std::move(callback));
    }

    /**
     * Update twin position via MQTT.
     *
     * @param twin_uuid UUID of the twin
     * @param position Position structure with x, y, z coordinates
     */
    void update_twin_position(const std::string& twin_uuid, const Position& position);

    /**
     * Backwards compatibility helper mirroring Python wrapper naming.
     */
    void publish_twin_position(const std::string& twin_uuid, double x, double y, double z);

    /**
     * Update twin rotation via MQTT.
     *
     * @param twin_uuid UUID of the twin
     * @param rotation Rotation structure (quaternion)
     */
    void update_twin_rotation(const std::string& twin_uuid, const Rotation& rotation);

    /**
     * Update twin scale via MQTT.
     *
     * @param twin_uuid UUID of the twin
     * @param scale Scale structure
     */
    void update_twin_scale(const std::string& twin_uuid, const Scale& scale);

    /**
     * Publish raw GPS data for a twin.
     *
     * The GPS payload is stored as a ``twin_gps_update`` telemetry event
     * in the backend database via Vector.  It does NOT update the twin's
     * rendered position — use update_twin_position for that.
     *
     * @param twin_uuid UUID of the twin
     * @param fix GPS fix data (latitude, longitude, altitude, satellite metadata)
     */
    void update_twin_gps(const std::string& twin_uuid, const GpsFix& fix);

    /**
     * Update joint state via MQTT.
     *
     * @param twin_uuid UUID of the twin
     * @param joint_name Name of the joint
     * @param state Joint state with optional position, velocity, and effort
     * @param source_type Source type for the message (defaults to SOURCE_TYPE_EDGE).
     */
    void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, const JointState& state,
                            const std::string& source_type = SOURCE_TYPE_EDGE);

    /**
     * Update a single joint state via MQTT with explicit timestamp metadata.
     *
     * @param twin_uuid UUID of the twin
     * @param joint_name Name of the joint
     * @param state Joint state with optional position, velocity, and effort
     * @param timestamp Unix timestamp in seconds; negative values use the current time
     * @param source_type Source type for the message (defaults to SOURCE_TYPE_EDGE).
     */
    void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, const JointState& state,
                            double timestamp, const std::string& source_type = SOURCE_TYPE_EDGE);

    /**
     * Update multiple joint states via MQTT in a single call.
     *
     * This helper iterates over the provided map and calls
     * `update_joint_state` for each joint. The order of delivery follows
     * the iteration order of the provided std::map.
     *
     * @param twin_uuid UUID of the twin
     * @param joints Map of joint name -> JointState
     * @param source_type Source type for the messages (defaults to SOURCE_TYPE_EDGE).
     *                    Must be one of: SOURCE_TYPE_EDGE, SOURCE_TYPE_TELE,
     *                    SOURCE_TYPE_EDIT, SOURCE_TYPE_SIM, SOURCE_TYPE_SIM_TELE
     */
    void update_joint_states(const std::string& twin_uuid, const std::map<std::string, JointState>& joints,
                             const std::string& source_type = SOURCE_TYPE_EDGE);

    /**
     * Update multiple joints at once using compact payload format.
     *
     * Publishes a single message:
     * {"source_type":"...", "<joint_name>": <position>, ...}
     */
    void update_joints_state(const std::string& twin_uuid, const std::map<std::string, double>& joint_positions,
                             const std::string& source_type = SOURCE_TYPE_EDGE);

    /**
     * Update multiple joints at once using the rich aggregated payload format.
     *
     * Publishes a single message with positions and optional velocities,
     * efforts, timestamp, workload_uuid, session_id, and source_subtype.
     */
    void update_joints_state(const std::string& twin_uuid, const std::map<std::string, double>& joint_positions,
                             const std::string& source_type, const std::map<std::string, double>& velocities,
                             const std::map<std::string, double>& efforts, double timestamp,
                             const std::string& workload_uuid, const std::string& session_id,
                             const std::string& source_subtype);

    // Environment-related methods

    /**
     * Subscribe to environment updates via MQTT.
     *
     * @param environment_uuid UUID of the environment
     * @param callback Callback function for updates
     */
    void subscribe_environment(const std::string& environment_uuid, MessageCallback callback);

    /**
     * Publish environment update via MQTT.
     *
     * @param environment_uuid UUID of the environment
     * @param update_type Type of update
     * @param data Update data
     */
    void publish_environment_update(const std::string& environment_uuid, const std::string& update_type,
                                    const json& data);

    // Telemetry lifecycle methods
    void publish_telemetry_start(const std::string& twin_uuid, const json& metadata = json::object());
    void publish_telemetry_start_message(const std::string& twin_uuid, const json& metadata = json::object());
    void publish_telemetry_end(const std::string& twin_uuid, const json& metadata = json::object());
    void publish_connected(const std::string& twin_uuid);
    void publish_disconnected(const std::string& twin_uuid);
    void publish_initial_observation(const std::string& twin_uuid, const json& observations, double fps = 30.0);

    // Streaming methods

    /**
     * Subscribe to video stream via MQTT.
     */
    void subscribe_video_stream(const std::string& twin_uuid, MessageCallback callback);

    /**
     * Subscribe to depth stream via MQTT.
     */
    void subscribe_depth_stream(const std::string& twin_uuid, MessageCallback callback);

    /**
     * Subscribe to point cloud stream via MQTT.
     */
    void subscribe_pointcloud_stream(const std::string& twin_uuid, MessageCallback callback);

    /**
     * Publish depth frame data via MQTT.
     */
    void publish_depth_frame(const std::string& twin_uuid, const json& depth_data);
    void publish_depth_frame(const std::string& twin_uuid, const json& depth_data, double timestamp);

    // WebRTC methods

    /**
     * Publish WebRTC signaling message via MQTT.
     */
    void publish_webrtc_message(const std::string& twin_uuid, const json& webrtc_data);

    /**
     * Subscribe to WebRTC signaling messages via MQTT.
     */
    void subscribe_webrtc_messages(const std::string& twin_uuid, MessageCallback callback);

    // Command methods

    /**
     * Publish Edge command response message via MQTT.
     */
    void publish_command_message(const std::string& twin_uuid, const std::string& status);
    void publish_command_message(const std::string& twin_uuid, const json& status_payload);

    /**
     * Subscribe to Edge command message via MQTT.
     */
    void subscribe_command_message(const std::string& twin_uuid, MessageCallback callback);

    // Connectivity testing

    /**
     * Send ping message to test connectivity.
     */
    void ping(const std::string& resource_uuid);

    /**
     * Subscribe to pong responses.
     */
    void subscribe_pong(const std::string& resource_uuid, MessageCallback callback);

    // Low-level MQTT methods for advanced use cases

    /**
     * Subscribe to any MQTT topic.
     *
     * @param topic MQTT topic pattern
     * @param callback Callback function for messages
     * @param qos Quality of service level (0, 1, or 2)
     */
    void subscribe(const std::string& topic, MessageCallback callback, int qos = 0);
    void subscribe(const std::string& topic, std::function<void(const json& message)> callback, int qos = 0);
    SubscriptionId subscribe_with_id(const std::string& topic, MessageCallback callback, int qos = 0);
    void unsubscribe(SubscriptionId subscription_id);

    /**
     * Publish a message to any MQTT topic.
     *
     * @param topic MQTT topic
     * @param message Message payload as JSON
     * @param qos Quality of service level (0, 1, or 2)
     */
    void publish(const std::string& topic, const json& message, int qos = 0);
    void publish(const std::string& topic, const std::string& message, int qos = 0);

private:
    // libmosquitto callback trampolines
    static void on_connect_cb(struct mosquitto* mosq, void* userdata, int rc);
    static void on_disconnect_cb(struct mosquitto* mosq, void* userdata, int rc);
    static void on_message_cb(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* msg);

    // Helper methods
    std::string generate_client_id();
    static std::string normalize_topic_prefix(const std::string& raw_prefix);
    std::string resolve_topic_prefix(const CyberwaveConfig& config) const;
    std::string with_prefix(const std::string& topic_suffix) const;
    bool parse_bool_env(const std::string& value) const;
    bool is_valid_source_type(const std::string& source_type) const;
    void resubscribe_registered_topics();
    void handle_twin_update_with_telemetry(const std::string& twin_uuid, const json& metadata = json::object());
    void publish_connect_message(const std::string& twin_uuid);
    void publish_disconnect_message(const std::string& twin_uuid);
    void handle_message(const std::string& topic, const std::string& payload);
    bool topic_matches(const std::string& topic, const std::string& pattern);
    std::vector<std::string> split_topic(const std::string& topic);

    // Member variables
    CyberwaveConfig config_;
    std::string mqtt_broker_;
    int mqtt_port_;
    std::string mqtt_username_;
    std::string mqtt_api_token_;
    std::string source_type_;
    std::string topic_prefix_;
    std::string client_id_;

    MosquittoPtr mosq_;
    std::atomic<bool> connected_{false};
    bool loop_started_ = false;
    std::mutex connect_mutex_;
    std::condition_variable connect_cv_;
    int reconnect_attempts_{0};
    int max_reconnect_attempts_{5};

    struct RegisteredCallback
    {
        SubscriptionId id;
        MessageCallback callback;
    };

    std::map<std::string, std::vector<RegisteredCallback>> message_callbacks_;
    std::map<std::string, int> subscription_qos_;
    std::mutex callbacks_mutex_;
    SubscriptionId next_subscription_id_{1};
    std::mutex telemetry_mutex_;
    std::vector<std::string> twin_uuids_;
    std::vector<std::string> twin_uuids_with_telemetry_start_;

    std::map<std::string, double> last_gps_update_times_;
    static constexpr double kGpsMinUpdateIntervalSec = 0.5;
};

} // namespace cyberwave
