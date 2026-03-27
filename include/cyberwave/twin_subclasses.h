/**
 * @brief Twin subclasses grouped by capability, plus capability-combination twins.
 */

#ifndef CYBERWAVE_TWIN_SUBCLASSES_H
#define CYBERWAVE_TWIN_SUBCLASSES_H

#include "cyberwave/twin.h"

#include <memory>
#include <string>

namespace cyberwave
{
struct IFrameSource;
struct IDepthSource;
class CameraStreamer;
class DepthStreamer;

/** @brief Capability flags used to choose the most specific twin subclass. */
struct Capabilities
{
    bool can_fly = false;
    bool can_locomote = false;
    bool can_grip = false;
    bool has_sensors = false; // RGB sensors
    bool has_depth = false;
};

/**
 * @brief Create the most appropriate twin subclass for a capability set.
 * @param client Owning SDK client.
 * @param uuid Twin UUID.
 * @param name Twin name.
 * @param caps Capability flags derived from backend metadata.
 * @return Unique pointer to a concrete `Twin` subtype.
 */
std::unique_ptr<Twin> create_twin(const Client& client, std::string uuid, std::string name, const Capabilities& caps);

// --- Subclasses (same identity as Twin; add capability-specific methods) ---

/** @brief Twin with RGB camera and streaming capabilities. */
class CameraTwin : public Twin
{
public:
    /** @brief Construct a camera-capable twin handle. */
    CameraTwin(const Client& client, std::string uuid, std::string name);

    /** @brief Destructor that stops active streaming if needed. */
    ~CameraTwin() override;

    /** Set frame source for streaming (e.g. VirtualFrameSource or OpenCV source). Must outlive
     * start_streaming/stop_streaming. */
    void set_frame_source(std::shared_ptr<IFrameSource> source);

    /** Start streaming; uses set_frame_source() if set, else throws. Requires client to have MQTT. */
    void start_streaming(int fps = 30, int camera_id = 0);

    /** Stop streaming. */
    void stop_streaming();

private:
    std::shared_ptr<IFrameSource> frame_source_;
    std::unique_ptr<CameraStreamer> streamer_;
};

/**
 * @brief Twin with RGB and depth camera capabilities.
 */
class DepthCameraTwin : public CameraTwin
{
public:
    /** @brief Construct a depth-camera-capable twin handle. */
    DepthCameraTwin(const Client& client, std::string uuid, std::string name);

    /** @brief Destructor that stops active depth streaming if needed. */
    ~DepthCameraTwin() override;

    // ---- Depth streaming ----

    /** Set depth frame source (e.g. VirtualDepthSource or RealSense wrapper). */
    void set_depth_source(std::shared_ptr<IDepthSource> source);

    /** Start depth streaming at given fps. Requires MQTT and a depth source. */
    void start_depth_streaming(int fps = 10);

    /** Stop depth streaming. */
    void stop_depth_streaming();

    // ---- Manual publish helpers ----

    /**
     * Publish a raw depth payload to {prefix}cyberwave/twin/{uuid}/depth.
     * @param json_payload Pre-formatted JSON string.
     */
    void publish_depth_frame(const std::string& json_payload);

    /**
     * Publish a point cloud payload to {prefix}cyberwave/twin/{uuid}/pointcloud.
     * @param json_payload Pre-formatted JSON string.
     */
    void publish_point_cloud(const std::string& json_payload);

    // ---- Advanced (not yet implemented) ----

    /** Not yet implemented — mirrors Python raise NotImplementedError. */
    [[noreturn]] void capture_depth_frame() const;

    /** Not yet implemented — mirrors Python raise NotImplementedError. */
    [[noreturn]] void get_point_cloud() const;

private:
    std::shared_ptr<IDepthSource> depth_source_;
    std::unique_ptr<DepthStreamer> depth_streamer_;
};

/** @brief Twin with locomotion commands such as forward and turn. */
class LocomoteTwin : public Twin
{
public:
    /** @brief Construct a locomotion-capable twin handle. */
    LocomoteTwin(const Client& client, std::string uuid, std::string name);

    /** Command to move forward (meters). Requires MQTT; no-op if MQTT not connected. */
    void move_forward(double distance_m, const std::string& source_type = "");

    /** Command to move backward (meters). */
    void move_backward(double distance_m, const std::string& source_type = "");

    /** Command to turn left (angle in radians). */
    void turn_left(double angle_rad = 1.5, const std::string& source_type = "");

    /** Command to turn right (angle in radians). */
    void turn_right(double angle_rad = 1.5, const std::string& source_type = "");
};

/** @brief Twin with flight commands such as takeoff, land, and hover. */
class FlyingTwin : public Twin
{
public:
    /** @brief Construct a flight-capable twin handle. */
    FlyingTwin(const Client& client, std::string uuid, std::string name);

    /** Take off to altitude (meters). Requires MQTT; no-op if not connected. */
    void takeoff(double altitude_m = 1.0);

    /** Land. */
    void land();

    /** Hover in place. */
    void hover();
};

/** @brief Twin with gripper commands such as grip and release. */
class GripperTwin : public Twin
{
public:
    /** @brief Construct a gripper-capable twin handle. */
    GripperTwin(const Client& client, std::string uuid, std::string name);

    /** Close gripper (force 0.0–1.0). Requires MQTT; no-op if not connected. */
    void grip(double force = 1.0);

    /** Open gripper. */
    void release();
};

/**
 * @brief Combination subclasses mirroring Python combo twin types.
 *
 * Each class inherits from a primary capability base and redeclares the
 * secondary capability methods so users still get a single concrete type
 * with the expected merged interface.
 */

/** @brief Twin combining gripper and RGB camera capabilities. */
class GripperCameraTwin : public CameraTwin
{
public:
    GripperCameraTwin(const Client& client, std::string uuid, std::string name);
    void grip(double force = 1.0);
    void release();
};

/** @brief Twin combining gripper and depth camera capabilities. */
class GripperDepthCameraTwin : public DepthCameraTwin
{
public:
    GripperDepthCameraTwin(const Client& client, std::string uuid, std::string name);
    void grip(double force = 1.0);
    void release();
};

/** @brief Twin combining locomotion and RGB camera capabilities. */
class LocomoteCameraTwin : public CameraTwin
{
public:
    LocomoteCameraTwin(const Client& client, std::string uuid, std::string name);
    void move_forward(double distance_m, const std::string& source_type = "");
    void move_backward(double distance_m, const std::string& source_type = "");
    void turn_left(double angle_rad = 1.5, const std::string& source_type = "");
    void turn_right(double angle_rad = 1.5, const std::string& source_type = "");
};

/** @brief Twin combining locomotion and depth camera capabilities. */
class LocomoteDepthCameraTwin : public DepthCameraTwin
{
public:
    LocomoteDepthCameraTwin(const Client& client, std::string uuid, std::string name);
    void move_forward(double distance_m, const std::string& source_type = "");
    void move_backward(double distance_m, const std::string& source_type = "");
    void turn_left(double angle_rad = 1.5, const std::string& source_type = "");
    void turn_right(double angle_rad = 1.5, const std::string& source_type = "");
};

/** @brief Twin combining locomotion and gripper capabilities. */
class LocomoteGripperTwin : public LocomoteTwin
{
public:
    LocomoteGripperTwin(const Client& client, std::string uuid, std::string name);
    void grip(double force = 1.0);
    void release();
};

/** @brief Twin combining locomotion, gripper, and RGB camera capabilities. */
class LocomoteGripperCameraTwin : public CameraTwin
{
public:
    LocomoteGripperCameraTwin(const Client& client, std::string uuid, std::string name);
    void move_forward(double distance_m, const std::string& source_type = "");
    void move_backward(double distance_m, const std::string& source_type = "");
    void turn_left(double angle_rad = 1.5, const std::string& source_type = "");
    void turn_right(double angle_rad = 1.5, const std::string& source_type = "");
    void grip(double force = 1.0);
    void release();
};

/** @brief Twin combining locomotion, gripper, and depth camera capabilities. */
class LocomoteGripperDepthCameraTwin : public DepthCameraTwin
{
public:
    LocomoteGripperDepthCameraTwin(const Client& client, std::string uuid, std::string name);
    void move_forward(double distance_m, const std::string& source_type = "");
    void move_backward(double distance_m, const std::string& source_type = "");
    void turn_left(double angle_rad = 1.5, const std::string& source_type = "");
    void turn_right(double angle_rad = 1.5, const std::string& source_type = "");
    void grip(double force = 1.0);
    void release();
};

/** @brief Twin combining flight and RGB camera capabilities. */
class FlyingCameraTwin : public CameraTwin
{
public:
    FlyingCameraTwin(const Client& client, std::string uuid, std::string name);
    void takeoff(double altitude_m = 1.0);
    void land();
    void hover();
};

/** @brief Twin combining flight and depth camera capabilities. */
class FlyingDepthCameraTwin : public DepthCameraTwin
{
public:
    FlyingDepthCameraTwin(const Client& client, std::string uuid, std::string name);
    void takeoff(double altitude_m = 1.0);
    void land();
    void hover();
};

/** @brief Twin combining flight and gripper capabilities. */
class FlyingGripperTwin : public FlyingTwin
{
public:
    FlyingGripperTwin(const Client& client, std::string uuid, std::string name);
    void grip(double force = 1.0);
    void release();
};

/** @brief Twin combining flight, gripper, and RGB camera capabilities. */
class FlyingGripperCameraTwin : public CameraTwin
{
public:
    FlyingGripperCameraTwin(const Client& client, std::string uuid, std::string name);
    void takeoff(double altitude_m = 1.0);
    void land();
    void hover();
    void grip(double force = 1.0);
    void release();
};

/** @brief Twin combining flight, gripper, and depth camera capabilities. */
class FlyingGripperDepthCameraTwin : public DepthCameraTwin
{
public:
    FlyingGripperDepthCameraTwin(const Client& client, std::string uuid, std::string name);
    void takeoff(double altitude_m = 1.0);
    void land();
    void hover();
    void grip(double force = 1.0);
    void release();
};

} // namespace cyberwave

#endif // CYBERWAVE_TWIN_SUBCLASSES_H
