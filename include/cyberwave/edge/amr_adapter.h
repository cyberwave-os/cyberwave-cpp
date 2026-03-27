/**
 * @brief Abstract AMR adapter interface used by Cyberwave AMR edge nodes.
 */

#ifndef CYBERWAVE_EDGE_AMR_ADAPTER_H
#define CYBERWAVE_EDGE_AMR_ADAPTER_H

#include "cyberwave/edge/amr_types.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cyberwave
{

/**
 * @brief Callback type for adapter status updates.
 */
using AMRStatusCallback = std::function<void(const std::string& action_id, const std::string& status,
                                             std::optional<std::string> message, std::optional<double> progress)>;

/**
 * @brief Interface that vendor-specific AMR adapters must implement.
 */
struct IAMRAdapter
{
    /** @brief Virtual destructor. */
    virtual ~IAMRAdapter() = default;

    /** @brief Connect to the underlying robot or vendor service. */
    virtual void connect() = 0;

    /** @brief Disconnect from the underlying robot or vendor service. */
    virtual void disconnect() = 0;

    /** @brief Return whether the adapter is currently connected. */
    virtual bool is_connected() const = 0;

    /** Poll current robot telemetry. */
    virtual std::optional<RobotTelemetry> poll_telemetry() = 0;

    /** Send navigation command. position/rotation/waypoints optional. Returns true if accepted. */
    virtual bool send_navigation_command(const std::string& action_id, const std::string& command,
                                         std::optional<Position3> position = std::nullopt,
                                         std::optional<RotationQuat> rotation = std::nullopt,
                                         std::vector<Position3> waypoints = {}) = 0;

    /** @brief Cancel an in-flight navigation action by action ID. */
    virtual bool cancel_navigation(const std::string& action_id) = 0;

    /** @brief Pause the current navigation action, if supported. */
    virtual bool pause_navigation() = 0;

    /** @brief Resume a paused navigation action, if supported. */
    virtual bool resume_navigation() = 0;

    /** Set callback for status updates (action_id, status, message, progress). */
    virtual void set_status_callback(AMRStatusCallback callback) = 0;
};

} // namespace cyberwave

#endif // CYBERWAVE_EDGE_AMR_ADAPTER_H
