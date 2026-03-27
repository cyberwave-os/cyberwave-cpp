/**
 * @brief Navigation plan builders and twin navigation commands.
 */

#ifndef CYBERWAVE_NAVIGATION_H
#define CYBERWAVE_NAVIGATION_H

#include "cyberwave/twin.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cyberwave
{

/**
 * @brief Builder for waypoint-based navigation plans.
 */
class NavigationPlan
{
public:
    /** @brief Construct a new plan with an auto-generated ID. */
    NavigationPlan();

    /** @brief Construct a new plan with an explicit plan ID. */
    explicit NavigationPlan(std::string plan_id);

    /** @brief Return the plan identifier. */
    const std::string& plan_id() const noexcept { return plan_id_; }
    /** @brief Return the plan name. */
    const std::string& name() const noexcept { return name_; }
    /** @brief Return the default controller policy UUID. */
    const std::string& controller_policy_uuid() const noexcept { return controller_policy_uuid_; }
    /** @brief Return plan metadata. */
    const std::map<std::string, std::string>& metadata() const noexcept { return metadata_; }

    /** Set a human-readable name for this plan. Mirrors Python set_name(). */
    NavigationPlan& set_name(std::string name);

    /** Attach a controller policy UUID. Mirrors Python with_controller(). */
    NavigationPlan& with_controller(std::string policy_uuid);

    /** Set arbitrary metadata key-value pairs. Mirrors Python set_metadata(**kw). */
    NavigationPlan& set_metadata(std::map<std::string, std::string> metadata);

    /**
     * @brief Add a simple waypoint.
     * @param x X position in meters.
     * @param y Y position in meters.
     * @param z Z position in meters.
     * @param yaw Optional yaw in radians.
     * @return Reference to this plan.
     */
    NavigationPlan& waypoint(double x, double y, double z, double yaw = 0.0);

    /**
     * Add a rich waypoint with optional id, quaternion rotation [w,x,y,z], and metadata JSON.
     * Mirrors Python NavigationPlan.waypoint(waypoint_id=, rotation=, metadata=).
     */
    NavigationPlan& waypoint(double x, double y, double z, double yaw, const std::string& id,
                             const std::vector<double>& rotation = {}, const std::string& metadata_json = "{}");

    /**
     * @brief Append several waypoints in one call.
     * @param waypoints Each inner vector is `[x, y, z]` or `[x, y, z, yaw]`.
     * @return Reference to this plan.
     */
    NavigationPlan& extend(const std::vector<std::vector<double>>& waypoints);

    /** @brief Build the plan as a vector-of-vectors waypoint payload. */
    std::vector<std::vector<double>> build_waypoints() const;

    /**
     * Build the full plan as a JSON string (id, name, controller_policy_uuid, metadata, waypoints).
     * Mirrors Python NavigationPlan.build().
     */
    std::string build() const;

    /** @brief Mission payload shape returned by `to_mission()`. */
    struct MissionPayload
    {
        std::string id;
        std::string name;
        std::string description;
        std::string twin_uuid;
        std::string controller_policy_uuid;
        std::string created_at;
        std::optional<bool> is_active;
        std::vector<std::vector<double>> waypoints;
        std::map<std::string, std::string> metadata;
    };

    /**
     * Convert to a full mission payload dict.
     * Mirrors Python NavigationPlan.to_mission() with extended kwargs.
     */
    MissionPayload to_mission(const std::string& twin_uuid, const std::string& mission_name = "",
                              const std::string& mission_id = "", const std::string& description = "",
                              const std::string& created_at = "", std::optional<bool> is_active = std::nullopt) const;

private:
    std::string plan_id_;
    std::string name_;
    std::string controller_policy_uuid_;
    std::map<std::string, std::string> metadata_;
    struct Waypoint
    {
        double x = 0, y = 0, z = 0;
        double yaw = 0;
        std::string id;
        std::vector<double> rotation; // [w, x, y, z] or empty
        std::string metadata_json = "{}";
    };
    std::vector<Waypoint> waypoints_;
};

/**
 * @brief Handle for issuing navigation commands to a twin.
 */
class TwinNavigationHandle
{
public:
    /** @brief Construct a navigation handle bound to a twin. */
    explicit TwinNavigationHandle(Twin twin);

    /** @brief Return the bound twin UUID. */
    const std::string& twin_uuid() const;

    /** Set default controller policy for subsequent commands. */
    TwinNavigationHandle& use_controller(const std::string& policy_uuid);

    /** Clear default controller. */
    TwinNavigationHandle& clear_controller();

    /** @brief Create a new plan builder with an auto-generated ID. */
    NavigationPlan plan() const;

    /** @brief Create a new plan builder with an explicit plan ID. */
    NavigationPlan plan(const std::string& plan_id) const;

    /** Navigate to position [x, y, z] with optional yaw, rotation quaternion, constraints and metadata. */
    void goto_position(double x, double y, double z, double yaw = 0.0, const std::string& environment_uuid = "",
                       const std::string& source_type = "", const std::vector<double>& rotation = {},
                       const std::string& constraints_json = "", const std::string& metadata_json = "") const;

    /** Follow a path (sequence of [x,y,z] waypoints) with optional wait, loops, constraints and metadata. */
    void follow_path(const std::vector<std::vector<double>>& waypoints, double wait_s = 0.0, int max_loops = 1,
                     const std::string& environment_uuid = "", const std::string& source_type = "",
                     const std::string& constraints_json = "", const std::string& metadata_json = "") const;

    /** Follow a built NavigationPlan with optional wait and loop count. */
    void follow_path(const NavigationPlan& plan, double wait_s = 0.0, int max_loops = 1) const;

    /** Stop navigation. Mirrors Python TwinNavigationHandle.stop(). */
    void stop(const std::string& environment_uuid = "", const std::string& source_type = "") const;

    /** Pause navigation. Mirrors Python TwinNavigationHandle.pause(). */
    void pause(const std::string& environment_uuid = "", const std::string& source_type = "") const;

    /** Resume navigation. Mirrors Python TwinNavigationHandle.resume(). */
    void resume(const std::string& environment_uuid = "", const std::string& source_type = "") const;

private:
    Twin twin_;
    std::string controller_policy_uuid_;
};

} // namespace cyberwave

#endif // CYBERWAVE_NAVIGATION_H
