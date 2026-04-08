#include "cyberwave/workers.h"

namespace cyberwave
{

std::vector<HookRegistration> HookRegistry::hooks() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return hooks_;
}

std::vector<SynchronizedGroup> HookRegistry::synchronized_groups() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return synchronized_;
}

void HookRegistry::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    hooks_.clear();
    synchronized_.clear();
}

HookRegistration HookRegistry::register_hook(const HookRegistration& registration)
{
    std::lock_guard<std::mutex> lock(mutex_);
    hooks_.push_back(registration);
    return hooks_.back();
}

SynchronizedGroup HookRegistry::register_synchronized(const SynchronizedGroup& group)
{
    std::lock_guard<std::mutex> lock(mutex_);
    synchronized_.push_back(group);
    return synchronized_.back();
}

HookRegistration HookRegistry::on_frame(const std::string& twin_uuid, HookCallback callback, const std::string& sensor,
                                        std::optional<int> fps)
{
    nlohmann::json options = nlohmann::json::object();
    if (fps)
        options["fps"] = *fps;
    return register_hook(
        make_registration("frames/" + sensor, "frame", twin_uuid, std::move(callback), sensor, options));
}

HookRegistration HookRegistry::on_depth(const std::string& twin_uuid, HookCallback callback, const std::string& sensor)
{
    return register_hook(make_registration("depth/" + sensor, "depth", twin_uuid, std::move(callback), sensor));
}

HookRegistration HookRegistry::on_audio(const std::string& twin_uuid, HookCallback callback, const std::string& sensor)
{
    return register_hook(make_registration("audio/" + sensor, "audio", twin_uuid, std::move(callback), sensor));
}

HookRegistration HookRegistry::on_pointcloud(const std::string& twin_uuid, HookCallback callback,
                                             const std::string& sensor)
{
    return register_hook(
        make_registration("pointcloud/" + sensor, "pointcloud", twin_uuid, std::move(callback), sensor));
}

HookRegistration HookRegistry::on_imu(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("imu", "imu", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_force_torque(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("force_torque", "force_torque", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_joint_states(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("joint_states", "joint_states", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_attitude(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("attitude", "attitude", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_gps(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("gps", "gps", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_end_effector_pose(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("end_effector_pose", "end_effector_pose", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_gripper_state(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("gripper_state", "gripper_state", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_map(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("map", "map", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_battery(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("battery", "battery", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_temperature(const std::string& twin_uuid, HookCallback callback)
{
    return register_hook(make_registration("temperature", "temperature", twin_uuid, std::move(callback)));
}

HookRegistration HookRegistry::on_lidar(const std::string& twin_uuid, HookCallback callback, const std::string& sensor)
{
    return register_hook(make_registration("lidar/" + sensor, "lidar", twin_uuid, std::move(callback), sensor));
}

HookRegistration HookRegistry::on_data(const std::string& twin_uuid, const std::string& channel, HookCallback callback)
{
    return register_hook(make_registration(channel, "data", twin_uuid, std::move(callback)));
}

SynchronizedGroup HookRegistry::on_synchronized(const std::string& twin_uuid, std::vector<std::string> channels,
                                                SynchronizedCallback callback, double tolerance_ms)
{
    return register_synchronized(
        SynchronizedGroup{std::move(channels), twin_uuid, std::move(callback), tolerance_ms, nlohmann::json::object()});
}

HookRegistration HookRegistry::make_registration(std::string channel, std::string hook_type, std::string twin_uuid,
                                                 HookCallback callback, std::string sensor_name, nlohmann::json options)
{
    return HookRegistration{
        std::move(channel),   std::move(twin_uuid),   std::move(callback),
        std::move(hook_type), std::move(sensor_name), std::move(options),
    };
}

} // namespace cyberwave
