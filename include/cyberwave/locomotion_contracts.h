/**
 * @brief Versioned locomotion payload contracts.
 */

#ifndef CYBERWAVE_LOCOMOTION_CONTRACTS_H
#define CYBERWAVE_LOCOMOTION_CONTRACTS_H

#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace cyberwave
{

inline constexpr const char* LOCOMOTION_VELOCITY_COMMAND_CONTRACT = "locomotion.velocity_command.v1";
inline constexpr std::array<const char*, 6> LOCOMOTION_VELOCITY_COMMAND_REQUIRED_FIELDS = {
    "contract", "linear_x", "angular_z", "duration_ms", "gait", "origin"};

struct LocomotionVelocityCommand;
inline void validate_locomotion_velocity_command(const LocomotionVelocityCommand& command, bool allow_stop = true);

struct LocomotionVelocityCommand
{
    double linear_x{0.0};
    double linear_y{0.0};
    double angular_z{0.0};
    int duration_ms{500};
    std::string gait{"walk"};
    std::string origin{"teleop"};

    std::string to_json() const
    {
        validate_locomotion_velocity_command(*this, true);

        std::ostringstream out;
        out << std::setprecision(17) << "{\"linear_x\":" << linear_x << ",\"linear_y\":" << linear_y
            << ",\"angular_z\":" << angular_z << ",\"duration_ms\":" << duration_ms << ",\"gait\":\"" << gait
            << "\",\"origin\":\"" << origin << "\",\"contract\":\"" << LOCOMOTION_VELOCITY_COMMAND_CONTRACT << "\"}";
        return out.str();
    }
};

inline bool is_valid_locomotion_gait(const std::string& gait)
{
    return gait == "walk" || gait == "trot" || gait == "stand";
}

inline bool is_valid_locomotion_origin(const std::string& origin)
{
    return origin == "teleop" || origin == "ai_policy" || origin == "navigation" || origin == "workflow";
}

inline void validate_locomotion_velocity_command(const LocomotionVelocityCommand& command, bool allow_stop)
{
    if (!std::isfinite(command.linear_x) || !std::isfinite(command.linear_y) || !std::isfinite(command.angular_z))
    {
        throw std::invalid_argument("Locomotion velocity command requires finite numeric velocities");
    }
    if (command.duration_ms == 0)
    {
        if (!allow_stop)
        {
            throw std::invalid_argument("duration_ms must be 0 or at least 50");
        }
    }
    else if (command.duration_ms < 50)
    {
        throw std::invalid_argument("duration_ms must be 0 or at least 50");
    }
    else if (command.duration_ms > 30000)
    {
        throw std::invalid_argument("duration_ms must be <= 30000");
    }
    if (!is_valid_locomotion_gait(command.gait))
    {
        throw std::invalid_argument("Invalid locomotion gait: " + command.gait);
    }
    if (!is_valid_locomotion_origin(command.origin))
    {
        throw std::invalid_argument("Invalid locomotion origin: " + command.origin);
    }
}

inline LocomotionVelocityCommand build_locomotion_velocity_command(double linear_x = 0.0, double linear_y = 0.0,
                                                                   double angular_z = 0.0, int duration_ms = 500,
                                                                   const std::string& gait = "walk",
                                                                   const std::string& origin = "teleop")
{
    LocomotionVelocityCommand command{linear_x, linear_y, angular_z, duration_ms, gait, origin};
    validate_locomotion_velocity_command(command, true);
    return command;
}

inline LocomotionVelocityCommand stop_locomotion_velocity_command(const std::string& origin = "teleop")
{
    LocomotionVelocityCommand command{0.0, 0.0, 0.0, 0, "stand", origin};
    validate_locomotion_velocity_command(command, true);
    return command;
}

} // namespace cyberwave

#endif // CYBERWAVE_LOCOMOTION_CONTRACTS_H
