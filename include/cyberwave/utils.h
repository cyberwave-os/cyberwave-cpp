/**
 * @brief Utility helpers for timing, device inspection, and fingerprint generation.
 */

#ifndef CYBERWAVE_UTILS_H
#define CYBERWAVE_UTILS_H

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace cyberwave
{

/**
 * @brief Synchronised wall-clock and monotonic time reference.
 */
class TimeReference
{
public:
    TimeReference();

    /** Update and return (wall_seconds, monotonic_seconds). */
    std::pair<double, double> update();

    /** Read current values without updating. */
    std::pair<double, double> read() const;

private:
    mutable std::mutex mutex_;
    double time_{0};
    double time_monotonic_{0};
};

/** @brief Device identity fields used for fingerprinting and diagnostics. */
struct DeviceInfo
{
    std::string hostname;
    std::string platform;
    std::string mac_address;
};

/** @brief Collect device information for fingerprinting. */
DeviceInfo get_device_info();

/**
 * @brief Generate a stable device fingerprint.
 */
std::string generate_fingerprint(const std::string& override = "");

/** @brief Format device information as a human-readable table. */
std::string format_device_info_table(std::optional<DeviceInfo> info = std::nullopt);

} // namespace cyberwave

#endif // CYBERWAVE_UTILS_H
