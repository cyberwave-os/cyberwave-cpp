#ifndef CYBERWAVE_WORKERS_H
#define CYBERWAVE_WORKERS_H

#include "cyberwave/data.h"

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cyberwave
{

struct HookContext
{
    double timestamp{0.0};
    std::string channel;
    std::string sensor_name{"default"};
    std::optional<std::string> twin_uuid;
    nlohmann::json metadata = nlohmann::json::object();
};

using HookCallback = std::function<void(const DataValue&, const HookContext&)>;
using SynchronizedSamples = std::unordered_map<std::string, DataSample>;
using SynchronizedCallback = std::function<void(const SynchronizedSamples&, const HookContext&)>;

struct HookRegistration
{
    std::string channel;
    std::string twin_uuid;
    HookCallback callback;
    std::string hook_type;
    std::string sensor_name{"default"};
    nlohmann::json options = nlohmann::json::object();
};

struct SynchronizedGroup
{
    std::vector<std::string> channels;
    std::string twin_uuid;
    SynchronizedCallback callback;
    double tolerance_ms{50.0};
    nlohmann::json options = nlohmann::json::object();
};

class HookRegistry
{
public:
    std::vector<HookRegistration> hooks() const;
    std::vector<SynchronizedGroup> synchronized_groups() const;
    void clear();

    HookRegistration register_hook(const HookRegistration& registration);
    SynchronizedGroup register_synchronized(const SynchronizedGroup& group);

    HookRegistration on_frame(const std::string& twin_uuid, HookCallback callback,
                              const std::string& sensor = "default", std::optional<int> fps = std::nullopt);
    HookRegistration on_depth(const std::string& twin_uuid, HookCallback callback,
                              const std::string& sensor = "default");
    HookRegistration on_audio(const std::string& twin_uuid, HookCallback callback,
                              const std::string& sensor = "default");
    HookRegistration on_pointcloud(const std::string& twin_uuid, HookCallback callback,
                                   const std::string& sensor = "default");
    HookRegistration on_imu(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_force_torque(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_joint_states(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_attitude(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_gps(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_end_effector_pose(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_gripper_state(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_map(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_battery(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_temperature(const std::string& twin_uuid, HookCallback callback);
    HookRegistration on_lidar(const std::string& twin_uuid, HookCallback callback,
                              const std::string& sensor = "default");
    HookRegistration on_data(const std::string& twin_uuid, const std::string& channel, HookCallback callback);

    SynchronizedGroup on_synchronized(const std::string& twin_uuid, std::vector<std::string> channels,
                                      SynchronizedCallback callback, double tolerance_ms = 50.0);

private:
    HookRegistration make_registration(std::string channel, std::string hook_type, std::string twin_uuid,
                                       HookCallback callback, std::string sensor_name = "default",
                                       nlohmann::json options = nlohmann::json::object());

    mutable std::mutex mutex_;
    std::vector<HookRegistration> hooks_;
    std::vector<SynchronizedGroup> synchronized_;
};

} // namespace cyberwave

#endif // CYBERWAVE_WORKERS_H
