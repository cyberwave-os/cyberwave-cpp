/**
 * @brief Twin motion handles for poses, animations, and scoped motion plans.
 */

#ifndef CYBERWAVE_MOTION_H
#define CYBERWAVE_MOTION_H

#include "cyberwave/twin.h"

#include <map>
#include <string>
#include <vector>

namespace cyberwave
{

/**
 * @brief Motion handle scoped to an environment, asset, or twin context.
 */
class ScopedMotionHandle
{
public:
    /**
     * @brief Construct a scoped motion handle.
     * @param parent Parent motion handle.
     * @param env_uuid Environment UUID used for scoped execution.
     * @param scope Scope name: `environment`, `asset`, or `twin`.
     */
    ScopedMotionHandle(Twin twin, std::string env_uuid, std::string scope = "environment");

    /** @brief Execute a pose within this scope. */
    void pose(const std::string& name = "", bool preview = false, bool sync = false,
              const std::string& source_type = "", int transition_ms = -1, int hold_ms = -1,
              const std::map<std::string, double>& joints = {}) const;

    /** @brief Execute an animation within this scope. */
    void animation(const std::string& name, bool preview = false, bool sync = false,
                   const std::string& source_type = "", int transition_ms = -1, int hold_ms = -1) const;

    /**
     * Execute a motion plan in this scope.
     * Mirrors Python ScopedMotionHandle.plan().
     */
    void plan(const std::string& plan_json, bool preview = false, bool sync = false,
              const std::string& source_type = "", int tick_ms = -1) const;

    /** @brief List keyframe names available in this scope. */
    std::vector<std::string> list_keyframes() const;

    /** @brief List animation names available in this scope. */
    std::vector<std::string> list_animations() const;

    /** @brief Return the scope name. */
    const std::string& scope() const noexcept { return scope_; }
    /** @brief Return the bound environment UUID, if any. */
    const std::string& environment_uuid() const noexcept { return env_uuid_; }

private:
    Twin twin_;
    std::string env_uuid_;
    std::string scope_;
};

/**
 * @brief Handle for listing and executing motions on a twin.
 */
class TwinMotionHandle
{
public:
    /** @brief Construct a motion handle bound to a twin. */
    explicit TwinMotionHandle(Twin twin);

    /** @brief Return the bound twin UUID. */
    const std::string& twin_uuid() const;

    /**
     * List keyframes for this twin (optional environment filter).
     * Returns keyframe data as JSON-like strings for simplicity; use rest API for full structure.
     */
    std::vector<std::string> list_keyframes(const std::string& environment_uuid = "",
                                            const std::string& scope = "auto") const;

    /**
     * List animation names for this twin (optional environment filter).
     * Mirrors Python TwinMotionHandle.list_animations().
     */
    std::vector<std::string> list_animations(const std::string& environment_uuid = "",
                                             const std::string& scope = "auto") const;

    /**
     * Execute a named pose with optional rich parameters.
     * Mirrors Python TwinMotionHandle.pose().
     */
    void pose(const std::string& name = "", const std::string& environment_uuid = "", bool preview = false,
              bool sync = false, const std::string& source_type = "", int transition_ms = -1, int hold_ms = -1,
              const std::map<std::string, double>& joints = {}, const std::string& scope = "auto") const;

    /**
     * Execute a named animation with optional rich parameters.
     * Mirrors Python TwinMotionHandle.animation().
     */
    void animation(const std::string& name, const std::string& environment_uuid = "", bool preview = false,
                   bool sync = false, const std::string& source_type = "", int transition_ms = -1, int hold_ms = -1,
                   const std::string& scope = "auto") const;

    /**
     * Execute a motion plan with optional rich parameters.
     * The plan_json argument is the JSON payload of the plan.
     * Mirrors Python TwinMotionHandle.plan().
     */
    void plan(const std::string& plan_json, const std::string& environment_uuid = "", bool preview = false,
              bool sync = false, const std::string& source_type = "", int tick_ms = -1,
              const std::string& scope = "auto") const;

    /**
     * Legacy overload: plan(action_type, plan_json).
     * Deprecated: use plan(plan_json, ...) with explicit named params instead.
     * The action_type arg is ignored; action_type is always "plan".
     */
    void plan_legacy(const std::string& action_type, const std::string& plan_json) const;

    /**
     * Return a ScopedMotionHandle that pre-fills environment_uuid for all calls.
     * Mirrors Python TwinMotionHandle.in_environment().
     */
    ScopedMotionHandle in_environment(const std::string& environment_uuid) const;

    /**
     * Return a ScopedMotionHandle scoped to the current environment (no specific uuid).
     * Mirrors Python TwinMotionHandle.environment property.
     */
    ScopedMotionHandle environment() const;

    /**
     * Return a ScopedMotionHandle scoped to the twin instance.
     * Mirrors Python TwinMotionHandle.twin property.
     */
    ScopedMotionHandle twin() const;

    /**
     * Return a ScopedMotionHandle scoped to the asset.
     * Mirrors Python TwinMotionHandle.asset property.
     */
    ScopedMotionHandle asset() const;

private:
    Twin twin_;
};

} // namespace cyberwave

#endif // CYBERWAVE_MOTION_H
