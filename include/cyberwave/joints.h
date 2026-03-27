/**
 * @brief Joint control helpers for reading and commanding twin joints.
 */

#ifndef CYBERWAVE_JOINTS_H
#define CYBERWAVE_JOINTS_H

#include "cyberwave/twin.h"

#include <map>
#include <string>
#include <vector>

namespace cyberwave
{

/**
 * @brief Controller for reading and commanding a twin's joints.
 */
class JointController
{
public:
    /** @brief Construct a joint controller bound to a twin. */
    explicit JointController(Twin twin);

    /** Refresh joint states from the server. */
    void refresh() const;

    /** Get current position of a joint (radians). */
    double get(const std::string& joint_name) const;

    /**
     * Set position of a joint (position in radians, or degrees if degrees=true).
     * Optional: timestamp (unix seconds) and source_type are forwarded to MQTT when connected.
     * Mirrors Python JointController.set().
     */
    void set(const std::string& joint_name, double position, bool degrees = false, double timestamp = -1.0,
             const std::string& source_type = "") const;

    /** List all joint names (after refresh). */
    std::vector<std::string> list() const;

    /** Get all joint states as name -> position (radians). */
    std::map<std::string, double> get_all() const;

private:
    Twin twin_;
};

} // namespace cyberwave

#endif // CYBERWAVE_JOINTS_H
