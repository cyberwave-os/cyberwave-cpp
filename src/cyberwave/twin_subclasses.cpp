#include "cyberwave/twin_subclasses.h"
#include "cyberwave/camera_streaming.h"
#include "cyberwave/client.h"
#include "cyberwave/constants.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/mqtt_interface.h"
#include "cyberwave/twin.h"

#include <chrono>
#include <sstream>

namespace cyberwave
{

namespace
{

std::unique_ptr<Twin> select_and_construct(const Client& client, std::string uuid, std::string name,
                                           const Capabilities& caps)
{
    // Combination types (most specific first)
    if (caps.can_fly && caps.can_grip && caps.has_depth)
        return std::make_unique<FlyingGripperDepthCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_fly && caps.can_grip && caps.has_sensors)
        return std::make_unique<FlyingGripperCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_fly && caps.can_grip)
        return std::make_unique<FlyingGripperTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_fly && caps.has_depth)
        return std::make_unique<FlyingDepthCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_fly && caps.has_sensors)
        return std::make_unique<FlyingCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_fly)
        return std::make_unique<FlyingTwin>(client, std::move(uuid), std::move(name));

    if (caps.can_locomote && caps.can_grip && caps.has_depth)
        return std::make_unique<LocomoteGripperDepthCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_locomote && caps.can_grip && caps.has_sensors)
        return std::make_unique<LocomoteGripperCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_locomote && caps.can_grip)
        return std::make_unique<LocomoteGripperTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_locomote && caps.has_depth)
        return std::make_unique<LocomoteDepthCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_locomote && caps.has_sensors)
        return std::make_unique<LocomoteCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_locomote)
        return std::make_unique<LocomoteTwin>(client, std::move(uuid), std::move(name));

    if (caps.can_grip && caps.has_depth)
        return std::make_unique<GripperDepthCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_grip && caps.has_sensors)
        return std::make_unique<GripperCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.can_grip)
        return std::make_unique<GripperTwin>(client, std::move(uuid), std::move(name));

    if (caps.has_depth)
        return std::make_unique<DepthCameraTwin>(client, std::move(uuid), std::move(name));
    if (caps.has_sensors)
        return std::make_unique<CameraTwin>(client, std::move(uuid), std::move(name));
    return std::make_unique<Twin>(client, std::move(uuid), std::move(name));
}

} // namespace

std::unique_ptr<Twin> create_twin(const Client& client, std::string uuid, std::string name, const Capabilities& caps)
{
    return select_and_construct(client, std::move(uuid), std::move(name), caps);
}

// --- CameraTwin ---
CameraTwin::CameraTwin(const Client& client, std::string uuid, std::string name)
    : Twin(client, std::move(uuid), std::move(name))
{
}

CameraTwin::~CameraTwin() { stop_streaming(); }

void CameraTwin::set_frame_source(std::shared_ptr<IFrameSource> source) { frame_source_ = std::move(source); }

void CameraTwin::start_streaming(int fps, int /* camera_id */)
{
    stop_streaming();
    if (!frame_source_)
        throw CyberwaveError(
            "Set a frame source with set_frame_source() before start_streaming (e.g. VirtualFrameSource)");
    auto mqtt = client().mqtt_client();
    if (!mqtt)
        throw CyberwaveError("Camera streaming requires client to have MQTT (set_mqtt_client)");
    streamer_ = std::make_unique<CameraStreamer>(mqtt, uuid(), frame_source_, fps);
    streamer_->start();
}

void CameraTwin::stop_streaming()
{
    if (streamer_)
    {
        streamer_->stop();
        streamer_.reset();
    }
}

// --- DepthCameraTwin ---
DepthCameraTwin::DepthCameraTwin(const Client& client, std::string uuid, std::string name)
    : CameraTwin(client, std::move(uuid), std::move(name))
{
}

DepthCameraTwin::~DepthCameraTwin() { stop_depth_streaming(); }

void DepthCameraTwin::set_depth_source(std::shared_ptr<IDepthSource> source) { depth_source_ = std::move(source); }

void DepthCameraTwin::start_depth_streaming(int fps)
{
    stop_depth_streaming();
    if (!depth_source_)
        throw CyberwaveError("Set a depth source with set_depth_source() before start_depth_streaming()");
    auto mqtt = client().mqtt_client();
    if (!mqtt)
        throw CyberwaveError("Depth streaming requires client to have MQTT (set_mqtt_client)");
    depth_streamer_ = std::make_unique<DepthStreamer>(mqtt, uuid(), depth_source_, fps);
    depth_streamer_->start();
}

void DepthCameraTwin::stop_depth_streaming()
{
    if (depth_streamer_)
    {
        depth_streamer_->stop();
        depth_streamer_.reset();
    }
}

void DepthCameraTwin::publish_depth_frame(const std::string& json_payload)
{
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string topic = mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/depth";
    mqtt->publish(topic, json_payload);
}

void DepthCameraTwin::publish_point_cloud(const std::string& json_payload)
{
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string topic = mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/pointcloud";
    mqtt->publish(topic, json_payload);
}

void DepthCameraTwin::capture_depth_frame() const
{
    throw CyberwaveError("capture_depth_frame() requires an active depth stream. "
                         "Use start_depth_streaming() first.");
}

void DepthCameraTwin::get_point_cloud() const
{
    throw CyberwaveError("get_point_cloud() requires depth sensor data processing. "
                         "This feature is not yet implemented.");
}

// --- LocomoteTwin ---
LocomoteTwin::LocomoteTwin(const Client& client, std::string uuid, std::string name)
    : Twin(client, std::move(uuid), std::move(name))
{
}

static void locomote_command(const Twin& twin, const std::string& command, double linear_x, double angular_z,
                             const std::string& source_type)
{
    auto mqtt = twin.client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string st = source_type.empty() ? SOURCE_TYPE_TELE : source_type;
    double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << st << "\",\"command\":\"" << command << "\",\"data\":{\"linear_x\":" << linear_x
        << ",\"angular_z\":" << angular_z << "},\"timestamp\":" << ts << "}";
    std::string topic = mqtt->get_topic_prefix() + "cyberwave/twin/" + twin.uuid() + "/command";
    mqtt->publish(topic, out.str());
}

void LocomoteTwin::move_forward(double distance_m, const std::string& source_type)
{
    locomote_command(*this, "move_forward", distance_m, 0.0, source_type);
}

void LocomoteTwin::move_backward(double distance_m, const std::string& source_type)
{
    locomote_command(*this, "move_backward", -distance_m, 0.0, source_type);
}

void LocomoteTwin::turn_left(double angle_rad, const std::string& source_type)
{
    locomote_command(*this, "turn_left", 0.0, angle_rad, source_type);
}

void LocomoteTwin::turn_right(double angle_rad, const std::string& source_type)
{
    locomote_command(*this, "turn_right", 0.0, -angle_rad, source_type);
}

// --- FlyingTwin ---
FlyingTwin::FlyingTwin(const Client& client, std::string uuid, std::string name)
    : Twin(client, std::move(uuid), std::move(name))
{
}

static void flying_command(const Twin& twin, const std::string& command, const std::string& payload)
{
    auto mqtt = twin.client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string topic = mqtt->get_topic_prefix() + "cyberwave/twin/" + twin.uuid() + "/command";
    double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << SOURCE_TYPE_TELE << "\",\"command\":\"" << command << "\",\"data\":" << payload
        << ",\"timestamp\":" << ts << "}";
    mqtt->publish(topic, out.str());
}

void FlyingTwin::takeoff(double altitude_m)
{
    std::ostringstream data;
    data << "{\"altitude\":" << altitude_m << "}";
    flying_command(*this, "takeoff", data.str());
}

void FlyingTwin::land() { flying_command(*this, "land", "{}"); }

void FlyingTwin::hover() { flying_command(*this, "hover", "{}"); }

// --- GripperTwin ---
GripperTwin::GripperTwin(const Client& client, std::string uuid, std::string name)
    : Twin(client, std::move(uuid), std::move(name))
{
}

static void gripper_command(const Twin& twin, const std::string& command, const std::string& payload)
{
    auto mqtt = twin.client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string topic = mqtt->get_topic_prefix() + "cyberwave/twin/" + twin.uuid() + "/command";
    double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << SOURCE_TYPE_TELE << "\",\"command\":\"" << command << "\",\"data\":" << payload
        << ",\"timestamp\":" << ts << "}";
    mqtt->publish(topic, out.str());
}

void GripperTwin::grip(double force)
{
    double f = (force < 0.0) ? 0.0 : (force > 1.0 ? 1.0 : force);
    std::ostringstream data;
    data << "{\"force\":" << f << "}";
    gripper_command(*this, "grip", data.str());
}

void GripperTwin::release() { gripper_command(*this, "release", "{}"); }

// ─────────────────────────────────────────────────────────────────
// Combination subclasses
// ─────────────────────────────────────────────────────────────────

// --- GripperCameraTwin ---
GripperCameraTwin::GripperCameraTwin(const Client& c, std::string uuid, std::string name)
    : CameraTwin(c, std::move(uuid), std::move(name))
{
}
void GripperCameraTwin::grip(double force)
{
    double f = (force < 0.0) ? 0.0 : (force > 1.0 ? 1.0 : force);
    std::ostringstream d;
    d << "{\"force\":" << f << "}";
    gripper_command(*this, "grip", d.str());
}
void GripperCameraTwin::release() { gripper_command(*this, "release", "{}"); }

// --- GripperDepthCameraTwin ---
GripperDepthCameraTwin::GripperDepthCameraTwin(const Client& c, std::string uuid, std::string name)
    : DepthCameraTwin(c, std::move(uuid), std::move(name))
{
}
void GripperDepthCameraTwin::grip(double force)
{
    double f = (force < 0.0) ? 0.0 : (force > 1.0 ? 1.0 : force);
    std::ostringstream d;
    d << "{\"force\":" << f << "}";
    gripper_command(*this, "grip", d.str());
}
void GripperDepthCameraTwin::release() { gripper_command(*this, "release", "{}"); }

// --- LocomoteCameraTwin ---
LocomoteCameraTwin::LocomoteCameraTwin(const Client& c, std::string uuid, std::string name)
    : CameraTwin(c, std::move(uuid), std::move(name))
{
}
void LocomoteCameraTwin::move_forward(double d, const std::string& st)
{
    locomote_command(*this, "move_forward", d, 0.0, st);
}
void LocomoteCameraTwin::move_backward(double d, const std::string& st)
{
    locomote_command(*this, "move_backward", -d, 0.0, st);
}
void LocomoteCameraTwin::turn_left(double a, const std::string& st)
{
    locomote_command(*this, "turn_left", 0.0, a, st);
}
void LocomoteCameraTwin::turn_right(double a, const std::string& st)
{
    locomote_command(*this, "turn_right", 0.0, -a, st);
}

// --- LocomoteDepthCameraTwin ---
LocomoteDepthCameraTwin::LocomoteDepthCameraTwin(const Client& c, std::string uuid, std::string name)
    : DepthCameraTwin(c, std::move(uuid), std::move(name))
{
}
void LocomoteDepthCameraTwin::move_forward(double d, const std::string& st)
{
    locomote_command(*this, "move_forward", d, 0.0, st);
}
void LocomoteDepthCameraTwin::move_backward(double d, const std::string& st)
{
    locomote_command(*this, "move_backward", -d, 0.0, st);
}
void LocomoteDepthCameraTwin::turn_left(double a, const std::string& st)
{
    locomote_command(*this, "turn_left", 0.0, a, st);
}
void LocomoteDepthCameraTwin::turn_right(double a, const std::string& st)
{
    locomote_command(*this, "turn_right", 0.0, -a, st);
}

// --- LocomoteGripperTwin ---
LocomoteGripperTwin::LocomoteGripperTwin(const Client& c, std::string uuid, std::string name)
    : LocomoteTwin(c, std::move(uuid), std::move(name))
{
}
void LocomoteGripperTwin::grip(double force)
{
    double f = (force < 0.0) ? 0.0 : (force > 1.0 ? 1.0 : force);
    std::ostringstream d;
    d << "{\"force\":" << f << "}";
    gripper_command(*this, "grip", d.str());
}
void LocomoteGripperTwin::release() { gripper_command(*this, "release", "{}"); }

// --- LocomoteGripperCameraTwin ---
LocomoteGripperCameraTwin::LocomoteGripperCameraTwin(const Client& c, std::string uuid, std::string name)
    : CameraTwin(c, std::move(uuid), std::move(name))
{
}
void LocomoteGripperCameraTwin::move_forward(double d, const std::string& st)
{
    locomote_command(*this, "move_forward", d, 0.0, st);
}
void LocomoteGripperCameraTwin::move_backward(double d, const std::string& st)
{
    locomote_command(*this, "move_backward", -d, 0.0, st);
}
void LocomoteGripperCameraTwin::turn_left(double a, const std::string& st)
{
    locomote_command(*this, "turn_left", 0.0, a, st);
}
void LocomoteGripperCameraTwin::turn_right(double a, const std::string& st)
{
    locomote_command(*this, "turn_right", 0.0, -a, st);
}
void LocomoteGripperCameraTwin::grip(double force)
{
    double f = (force < 0.0) ? 0.0 : (force > 1.0 ? 1.0 : force);
    std::ostringstream d;
    d << "{\"force\":" << f << "}";
    gripper_command(*this, "grip", d.str());
}
void LocomoteGripperCameraTwin::release() { gripper_command(*this, "release", "{}"); }

// --- LocomoteGripperDepthCameraTwin ---
LocomoteGripperDepthCameraTwin::LocomoteGripperDepthCameraTwin(const Client& c, std::string uuid, std::string name)
    : DepthCameraTwin(c, std::move(uuid), std::move(name))
{
}
void LocomoteGripperDepthCameraTwin::move_forward(double d, const std::string& st)
{
    locomote_command(*this, "move_forward", d, 0.0, st);
}
void LocomoteGripperDepthCameraTwin::move_backward(double d, const std::string& st)
{
    locomote_command(*this, "move_backward", -d, 0.0, st);
}
void LocomoteGripperDepthCameraTwin::turn_left(double a, const std::string& st)
{
    locomote_command(*this, "turn_left", 0.0, a, st);
}
void LocomoteGripperDepthCameraTwin::turn_right(double a, const std::string& st)
{
    locomote_command(*this, "turn_right", 0.0, -a, st);
}
void LocomoteGripperDepthCameraTwin::grip(double force)
{
    double f = (force < 0.0) ? 0.0 : (force > 1.0 ? 1.0 : force);
    std::ostringstream d;
    d << "{\"force\":" << f << "}";
    gripper_command(*this, "grip", d.str());
}
void LocomoteGripperDepthCameraTwin::release() { gripper_command(*this, "release", "{}"); }

// --- FlyingCameraTwin ---
FlyingCameraTwin::FlyingCameraTwin(const Client& c, std::string uuid, std::string name)
    : CameraTwin(c, std::move(uuid), std::move(name))
{
}
void FlyingCameraTwin::takeoff(double alt)
{
    std::ostringstream d;
    d << "{\"altitude\":" << alt << "}";
    flying_command(*this, "takeoff", d.str());
}
void FlyingCameraTwin::land() { flying_command(*this, "land", "{}"); }
void FlyingCameraTwin::hover() { flying_command(*this, "hover", "{}"); }

// --- FlyingDepthCameraTwin ---
FlyingDepthCameraTwin::FlyingDepthCameraTwin(const Client& c, std::string uuid, std::string name)
    : DepthCameraTwin(c, std::move(uuid), std::move(name))
{
}
void FlyingDepthCameraTwin::takeoff(double alt)
{
    std::ostringstream d;
    d << "{\"altitude\":" << alt << "}";
    flying_command(*this, "takeoff", d.str());
}
void FlyingDepthCameraTwin::land() { flying_command(*this, "land", "{}"); }
void FlyingDepthCameraTwin::hover() { flying_command(*this, "hover", "{}"); }

// --- FlyingGripperTwin ---
FlyingGripperTwin::FlyingGripperTwin(const Client& c, std::string uuid, std::string name)
    : FlyingTwin(c, std::move(uuid), std::move(name))
{
}
void FlyingGripperTwin::grip(double force)
{
    double f = (force < 0.0) ? 0.0 : (force > 1.0 ? 1.0 : force);
    std::ostringstream d;
    d << "{\"force\":" << f << "}";
    gripper_command(*this, "grip", d.str());
}
void FlyingGripperTwin::release() { gripper_command(*this, "release", "{}"); }

// --- FlyingGripperCameraTwin ---
FlyingGripperCameraTwin::FlyingGripperCameraTwin(const Client& c, std::string uuid, std::string name)
    : CameraTwin(c, std::move(uuid), std::move(name))
{
}
void FlyingGripperCameraTwin::takeoff(double alt)
{
    std::ostringstream d;
    d << "{\"altitude\":" << alt << "}";
    flying_command(*this, "takeoff", d.str());
}
void FlyingGripperCameraTwin::land() { flying_command(*this, "land", "{}"); }
void FlyingGripperCameraTwin::hover() { flying_command(*this, "hover", "{}"); }
void FlyingGripperCameraTwin::grip(double force)
{
    double f = (force < 0.0) ? 0.0 : (force > 1.0 ? 1.0 : force);
    std::ostringstream d;
    d << "{\"force\":" << f << "}";
    gripper_command(*this, "grip", d.str());
}
void FlyingGripperCameraTwin::release() { gripper_command(*this, "release", "{}"); }

// --- FlyingGripperDepthCameraTwin ---
FlyingGripperDepthCameraTwin::FlyingGripperDepthCameraTwin(const Client& c, std::string uuid, std::string name)
    : DepthCameraTwin(c, std::move(uuid), std::move(name))
{
}
void FlyingGripperDepthCameraTwin::takeoff(double alt)
{
    std::ostringstream d;
    d << "{\"altitude\":" << alt << "}";
    flying_command(*this, "takeoff", d.str());
}
void FlyingGripperDepthCameraTwin::land() { flying_command(*this, "land", "{}"); }
void FlyingGripperDepthCameraTwin::hover() { flying_command(*this, "hover", "{}"); }
void FlyingGripperDepthCameraTwin::grip(double force)
{
    double f = (force < 0.0) ? 0.0 : (force > 1.0 ? 1.0 : force);
    std::ostringstream d;
    d << "{\"force\":" << f << "}";
    gripper_command(*this, "grip", d.str());
}
void FlyingGripperDepthCameraTwin::release() { gripper_command(*this, "release", "{}"); }

} // namespace cyberwave
