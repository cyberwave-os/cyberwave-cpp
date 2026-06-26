/**
 * @brief High-level twin handle used for control, queries, and subscriptions.
 */

#ifndef CYBERWAVE_TWIN_H
#define CYBERWAVE_TWIN_H

#include "cyberwave/mqtt_interface.h"
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cyberwave/assets.h"
#include "cyberwave/locomotion_contracts.h"

namespace cyberwave
{

class Client;
class TwinAlertManager;
class TwinNavigationHandle;
class TwinMotionHandle;
class JointController;
class TwinControllerHandle;
struct IFrameSource;
struct IDepthSource;
class CameraStreamer;
class DepthStreamer;

/**
 * @brief Handle to a twin returned by `Client` or `TwinManager`.
 */
class Twin
{
public:
    /** @brief Construct a lightweight twin handle from identity fields. */
    Twin(const Client& client, std::string uuid, std::string name);

    /** @brief Construct a twin handle from a full backend schema payload. */
    Twin(const Client& client, std::shared_ptr<void> schema_ptr);

    /** @brief Virtual destructor for subclassed twin types. */
    virtual ~Twin() = default;

    /** @brief Return the owning SDK client. */
    const Client& client() const noexcept { return client_.get(); }
    /** @brief Return the twin UUID. */
    const std::string& uuid() const noexcept { return uuid_; }
    /** @brief Return the twin name. */
    const std::string& name() const noexcept { return name_; }

    /** Optional environment UUID (may be empty). */
    const std::string& environment_id() const noexcept { return environment_id_; }
    void set_environment_id(std::string id) { environment_id_ = std::move(id); }

    /** @brief Return the alert manager scoped to this twin. */
    TwinAlertManager alerts() const;

    /** @brief Return the navigation handle for this twin. */
    TwinNavigationHandle navigation() const;

    /** @brief Return the motion handle for this twin. */
    TwinMotionHandle motion() const;

    /** @brief Return the joint controller for this twin. */
    JointController joints() const;

    /** @brief Return the controller handle for keyboard teleoperation helpers. */
    TwinControllerHandle controller() const;

    /** UUID of the asset this twin was created from (empty if not loaded from full schema). */
    std::string asset_id() const;

    /** Whether the twin currently uses a fixed base. */
    bool fixed_base() const;

    /** Capabilities map as a JSON object string ("{}") if none. */
    std::string capabilities_json() const;

    /** UUIDs of child twins (requires full schema from TwinManager::get()). */
    std::vector<std::string> child_uuids() const;

    /**
     * Fetch the parent Twin (if docked). Returns nullopt if not docked.
     * Reads attach_to_twin_uuid from the schema, then fetches from TwinManager.
     * Mirrors Python Twin.parent.
     */
    std::optional<Twin> parent() const;

    /**
     * Fetch child Twin objects. Iterates child_uuids() and fetches each.
     * Mirrors Python Twin.children.
     */
    std::vector<Twin> children() const;

    /** True if the named capability is present. */
    bool has_capability(const std::string& cap) const;

    /** True if any camera/sensor capability is present. Pass specific sensor_type to check that type. */
    bool has_sensor(const std::string& sensor_type = "") const;

    /** True if the twin advertises aerial control support. */
    bool can_fly() const;

    /** True if the twin advertises locomotion support. */
    bool can_locomote() const;

    /** True if the twin advertises gripper support. */
    bool can_grip() const;

    /**
     * Re-fetch this twin from the backend and update internal state.
     * Mirrors Python Twin.refresh().
     */
    void refresh();

    /**
     * Delete this twin from the backend.
     * Mirrors Python Twin.delete().
     */
    void delete_twin();

    /**
     * Move twin to position [x, y, z] (metres). Delegates to TwinManager::update_state().
     * Mirrors Python Twin.edit_position(x, y, z).
     */
    void edit_position(double x, double y, double z);

    /**
     * Set twin rotation as quaternion [w, x, y, z].
     * Mirrors Python Twin.edit_rotation(quaternion=...).
     */
    void edit_rotation(double w, double rx, double ry, double rz);

    /**
     * Set twin rotation from Euler angles (degrees).
     * Converts yaw/pitch/roll to quaternion and delegates to edit_rotation(w,x,y,z).
     * Mirrors Python Twin.edit_rotation(yaw=, pitch=, roll=).
     */
    void edit_rotation(double yaw, double pitch = 0.0, double roll = 0.0);

    /**
     * Uniformly scale twin. Not supported by the REST API — throws CyberwaveError.
     * Stub to mirror Python Twin.edit_scale() interface.
     */
    void edit_scale(double x, double y, double z);

    // --- Joint states (delegate to TwinManager) ---
    /** Get joint states as a map from joint name to position. */
    std::map<std::string, double> get_joint_states() const;

    /** Update a single joint state.
     *
     * Pass ``std::nullopt`` (the default) for ``velocity`` / ``effort`` when the
     * channel is not commanded — drivers treat absent channels as "do not
     * command". An explicitly supplied value, including ``0.0``, is forwarded.
     */
    void update_joint_state(const std::string& joint_name, double position,
                            std::optional<double> velocity = std::nullopt,
                            std::optional<double> effort = std::nullopt) const;

    // --- Calibration (delegate to TwinManager) ---
    /**
     * Get joint calibration as a JSON string.
     * robot_type optionally filters by robot type (e.g. "amr", "arm").
     * Note: robot_type is currently ignored by the generated REST client.
     */
    std::string get_calibration(const std::string& robot_type = "") const;

    /** Update joint calibration. Returns updated calibration as JSON. */
    std::string update_calibration(const std::string& calibration_json, const std::string& robot_type = "") const;

    // --- Frame (delegate to TwinManager) ---
    /**
     * Get latest RGB frame.
     * sensor_id selects a specific camera on multi-camera twins (e.g. "wrist_camera").
     */
    std::vector<unsigned char> get_latest_frame(bool mock = false, const std::string& sensor_id = "",
                                                const std::string& source_type = "") const;

    // --- Capability convenience helpers ---

    /** Set frame source for streaming (e.g. VirtualFrameSource or OpenCV source). */
    void set_frame_source(std::shared_ptr<IFrameSource> source);

    /** Start RGB streaming. Requires sensor capability, MQTT, and a frame source. */
    void start_streaming(int fps = 30, int camera_id = 0);

    /** Stop RGB streaming started via start_streaming(). */
    void stop_streaming();

    /** Set depth frame source for depth streaming. */
    void set_depth_source(std::shared_ptr<IDepthSource> source);

    /** Start depth streaming. Requires depth capability, MQTT, and a depth source. */
    void start_depth_streaming(int fps = 10);

    /** Stop depth streaming started via start_depth_streaming(). */
    void stop_depth_streaming();

    /** Publish a raw depth frame JSON payload. */
    void publish_depth_frame(const std::string& json_payload);

    /** Publish a raw point cloud JSON payload. */
    void publish_point_cloud(const std::string& json_payload);

    /** Not yet implemented, kept for Python parity. */
    [[noreturn]] void capture_depth_frame() const;

    /** Not yet implemented, kept for Python parity. */
    [[noreturn]] void get_point_cloud() const;

    /** Command forward locomotion over MQTT. */
    void move_forward(double distance_m, const std::string& source_type = "");

    /** Command backward locomotion over MQTT. */
    void move_backward(double distance_m, const std::string& source_type = "");

    /** Command a left turn over MQTT. */
    void turn_left(double angle_rad = 1.5, const std::string& source_type = "");

    /** Command a right turn over MQTT. */
    void turn_right(double angle_rad = 1.5, const std::string& source_type = "");

    /**
     * Dispatch a backend-owned locomotion velocity policy.
     *
     * The command must use the shared locomotion.velocity_command.v1 contract.
     * Returns the raw Control Agent dispatch response JSON.
     */
    std::string dispatch_velocity(const LocomotionVelocityCommand& command, const std::string& mode = "",
                                  const std::string& simulation_backend = "",
                                  const std::string& controller_policy_uuid = "") const;

    /** Dispatch a velocity command through a specific backend PolicyRef. */
    std::string dispatch_velocity(const LocomotionVelocityCommand& command, const PolicyRefPayload& policy_ref,
                                  const std::string& mode = "", const std::string& simulation_backend = "") const;

    /** Build and dispatch a backend-owned velocity command through the resolved policy. */
    std::string set_velocity(double linear_x = 0.0, double linear_y = 0.0, double angular_z = 0.0,
                             int duration_ms = 500, const std::string& gait = "walk",
                             const std::string& origin = "teleop", const std::string& mode = "",
                             const std::string& simulation_backend = "",
                             const std::string& controller_policy_uuid = "") const;

    /** Drive forward through the backend-resolved velocity policy. */
    std::string drive_forward(double speed = 0.25, int duration_ms = 1000, const std::string& mode = "",
                              const std::string& simulation_backend = "",
                              const std::string& controller_policy_uuid = "") const;

    /** Drive backward through the backend-resolved velocity policy. */
    std::string drive_backward(double speed = 0.25, int duration_ms = 1000, const std::string& mode = "",
                               const std::string& simulation_backend = "",
                               const std::string& controller_policy_uuid = "") const;

    /** Turn left through the backend-resolved velocity policy. */
    std::string turn_velocity_left(double angular = 0.5, int duration_ms = 1000, const std::string& mode = "",
                                   const std::string& simulation_backend = "",
                                   const std::string& controller_policy_uuid = "") const;

    /** Turn right through the backend-resolved velocity policy. */
    std::string turn_velocity_right(double angular = 0.5, int duration_ms = 1000, const std::string& mode = "",
                                    const std::string& simulation_backend = "",
                                    const std::string& controller_policy_uuid = "") const;

    /** Dispatch a canonical zero-velocity stop command through the resolved policy. */
    std::string stop_velocity(const std::string& mode = "", const std::string& simulation_backend = "",
                              const std::string& controller_policy_uuid = "") const;

    /** Command aerial takeoff over MQTT. */
    void takeoff(double altitude_m = 1.0);

    /** Command landing over MQTT. */
    void land();

    /** Command hover over MQTT. */
    void hover();

    /** Command gripper close over MQTT. */
    void grip(double force = 1.0);

    /** Command gripper open over MQTT. */
    void release();

    // --- Universal schema (delegate to TwinManager) ---
    /** Get the universal schema at path (JSON string). */
    std::string get_schema(const std::string& path = "") const;

    /** Patch the universal schema. Returns updated schema as JSON. */
    std::string update_schema(const std::string& path, const std::string& value_json,
                              const std::string& op = "replace") const;

    /**
     * Get controllable joint names (revolute, prismatic, continuous).
     * Parses get_schema() for movable joint types.
     * Mirrors Python Twin.get_controllable_joint_names().
     */
    std::vector<std::string> get_controllable_joint_names() const;

    // --- MQTT subscribe helpers (require set_mqtt_client on Client) ---

    /**
     * Subscribe to all updates for this twin (cyberwave/twin/{uuid}/#).
     * Requires MQTT client set on the parent Client.
     * Mirrors Python Twin.subscribe().
     */
    void subscribe(MqttMessageHandler on_update) const;

    /**
     * Subscribe to position updates for this twin.
     * Mirrors Python Twin.subscribe_position().
     */
    void subscribe_position(MqttMessageHandler on_update) const;

    /**
     * Subscribe to rotation updates for this twin.
     * Mirrors Python Twin.subscribe_rotation().
     */
    void subscribe_rotation(MqttMessageHandler on_update) const;

    /**
     * Subscribe to joint state updates for this twin.
     * Mirrors Python Twin.subscribe_joints().
     */
    void subscribe_joints(MqttMessageHandler on_update) const;

private:
    std::reference_wrapper<const Client> client_;
    std::string uuid_;
    std::string name_;
    std::string environment_id_;

    /** Optional full TwinSchema (set by schema constructor or refresh()). */
    std::shared_ptr<void> schema_;
    std::shared_ptr<IFrameSource> frame_source_;
    std::shared_ptr<CameraStreamer> camera_streamer_;
    std::shared_ptr<IDepthSource> depth_source_;
    std::shared_ptr<DepthStreamer> depth_streamer_;
};

} // namespace cyberwave

#endif // CYBERWAVE_TWIN_H
