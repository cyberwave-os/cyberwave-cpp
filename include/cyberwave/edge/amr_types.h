/**
 * @brief Shared AMR edge-node enums, structs, and adapter configuration types.
 */

#ifndef CYBERWAVE_EDGE_AMR_TYPES_H
#define CYBERWAVE_EDGE_AMR_TYPES_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cyberwave
{

/** @brief Robot operational states. */
enum class RobotState
{
    Idle,
    Navigating,
    Executing,
    Paused,
    Error,
    Charging,
    Teleop
};

/** @brief Navigation action statuses. */
enum class NavigationStatus
{
    Queued,
    Running,
    Completed,
    Failed,
    Cancelled
};

inline const char* to_string(RobotState s)
{
    switch (s)
    {
        case RobotState::Idle:
            return "idle";
        case RobotState::Navigating:
            return "navigating";
        case RobotState::Executing:
            return "executing";
        case RobotState::Paused:
            return "paused";
        case RobotState::Error:
            return "error";
        case RobotState::Charging:
            return "charging";
        case RobotState::Teleop:
            return "teleop";
    }
    return "idle";
}

inline const char* to_string(NavigationStatus s)
{
    switch (s)
    {
        case NavigationStatus::Queued:
            return "queued";
        case NavigationStatus::Running:
            return "running";
        case NavigationStatus::Completed:
            return "completed";
        case NavigationStatus::Failed:
            return "failed";
        case NavigationStatus::Cancelled:
            return "cancelled";
    }
    return "queued";
}

/** @brief Configuration for AMR protocol adapters. */
struct AdapterConfig
{
    std::string adapter_type;
    std::string host;
    std::uint16_t port = 0;
    std::string username;
    std::string password;
    std::string api_key;
    std::string robot_id;
    double position_poll_rate_hz = 10.0;
    double telemetry_poll_rate_hz = 1.0;
    std::string vda_manufacturer;
    std::string vda_serial_number;
    std::map<std::string, std::string> extra;

    static AdapterConfig from_env();
};

/** @brief Cartesian position in metres. */
struct Position3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/** @brief Quaternion rotation in `{w, x, y, z}` order. */
struct RotationQuat
{
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/** @brief Snapshot of robot telemetry values. */
struct RobotTelemetry
{
    std::optional<Position3> position;
    std::optional<RotationQuat> rotation;
    std::optional<double> velocity_linear;
    std::optional<double> velocity_angular;
    std::optional<double> battery_level; // 0–100%
    bool battery_charging = false;
    RobotState state = RobotState::Idle;
    std::vector<std::map<std::string, std::string>> errors;
    std::optional<std::string> current_action_id;
    std::optional<double> action_progress; // 0–100%
    std::map<std::string, std::string> vendor_data;
};

} // namespace cyberwave

#endif // CYBERWAVE_EDGE_AMR_TYPES_H
