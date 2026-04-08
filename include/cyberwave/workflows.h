/**
 * @brief Workflow views and workflow execution helpers for the Cyberwave SDK.
 */

#ifndef CYBERWAVE_WORKFLOWS_H
#define CYBERWAVE_WORKFLOWS_H

#include "cyberwave/mqtt_interface.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;
class WorkflowRun;

/**
 * @brief View over a workflow definition.
 */
class Workflow
{
public:
    /** @brief Construct a workflow view from a backend payload. */
    Workflow(const Client& client, std::shared_ptr<void> schema_ptr);

    /** @brief Return the workflow UUID. */
    std::string uuid() const;

    /** @brief Return the workspace-scoped slug. */
    std::string slug() const;

    /** @brief Return the workflow name. */
    std::string name() const;

    /** @brief Return the workflow description. */
    std::string description() const;

    /** @brief Return whether the workflow is active. */
    bool is_active() const;

    /** Human-friendly status: "active" or "inactive". */
    std::string status() const;

    /** @brief Return the owning workspace UUID. */
    std::string workspace_uuid() const;

    /** Visibility string (e.g. "public", "private"). */
    std::string visibility() const;

    /** ISO timestamp string of creation time (empty if not set). */
    std::string created_at() const;

    /** ISO timestamp string of last update (empty if not set). */
    std::string updated_at() const;

    /** Metadata as a JSON object string (empty object "{}" if none). */
    std::string metadata_json() const;

    /** @brief Trigger a new workflow run using string inputs. */
    WorkflowRun trigger(const std::map<std::string, std::string>& inputs = {}) const;

    /**
     * Trigger a new run with pre-serialized JSON inputs.
     * inputs_json must be a JSON object string, e.g. "{\"key\":42}".
     * Mirrors Python Workflow.trigger() when called with arbitrary value types.
     */
    WorkflowRun trigger_with_json(const std::string& inputs_json) const;

    /** @brief List runs for this workflow, optionally filtered by status. */
    std::vector<WorkflowRun> runs(const std::string& status_filter = "") const;

private:
    std::reference_wrapper<const Client> client_;
    std::shared_ptr<void> schema_;
};

/**
 * @brief View over a single workflow run.
 */
class WorkflowRun
{
public:
    /** @brief Construct a workflow run view from a backend payload. */
    WorkflowRun(const Client& client, std::shared_ptr<void> schema_ptr);

    /** @brief Return the run UUID. */
    std::string uuid() const;

    /** @brief Return the parent workflow UUID. */
    std::string workflow_id() const;

    /** @brief Return the run status. */
    std::string status() const;

    // Extra data getters (mirrors Python WorkflowRun properties)
    std::string inputs_json() const; ///< Inputs as a JSON string (empty if none)
    std::string result_json() const; ///< Result as a JSON string (empty if none)
    std::string error() const;       ///< Error message (empty if none)
    std::string started_at() const;  ///< ISO timestamp string (empty if not set)
    std::string finished_at() const; ///< ISO timestamp string (empty if not set)
    /** Alias for finished_at(), mirrors Python completed_at. */
    std::string completed_at() const { return finished_at(); }

    /**
     * True if status is a terminal state: success, error, failed, completed, canceled, cancelled.
     * Mirrors Python WorkflowRun.is_terminal.
     */
    bool is_terminal() const;

    /**
     * Duration in seconds between started_at and finished_at.
     * Returns -1.0 if not yet finished or timestamps unavailable.
     * Mirrors Python WorkflowRun.duration.
     */
    double duration() const;

    /** @brief Refresh this run from the backend. */
    WorkflowRun& refresh();

    /** @brief Cancel this run and refresh local state. */
    WorkflowRun& cancel();

    /**
     * Subscribe to MQTT status updates for this run.
     * The callback receives the new status string and an updated run view.
     * Keep the returned handle alive for as long as you want to receive updates.
     * Requires an MQTT client set on the parent Client.
     */
    [[nodiscard]] std::unique_ptr<MqttSubscriptionHandle>
    on_status_change(const std::function<void(const std::string&, const WorkflowRun&)>& callback);

    /**
     * Block until the run reaches a terminal state or timeout_s seconds have passed.
     * Returns *this. Throws CyberwaveTimeoutError if timeout exceeded.
     * Mirrors Python WorkflowRun.wait(timeout, poll_interval).
     */
    WorkflowRun& wait(double timeout_s = 60.0, double poll_interval_s = 1.0);

private:
    std::reference_wrapper<const Client> client_;
    std::shared_ptr<void> schema_;
    std::weak_ptr<void> client_lifetime_;
};

/**
 * @brief Manager for workflow lookup and triggering.
 */
class WorkflowManager
{
public:
    /** @brief Construct a workflow manager bound to a client. */
    explicit WorkflowManager(const Client& client);

    /** @brief List all accessible workflows. */
    std::vector<Workflow> list() const;

    /** @brief Fetch a workflow by UUID. */
    Workflow get(const std::string& workflow_id) const;

    /** @brief Fetch a workflow by workspace UUID and slug. */
    Workflow get_by_slug(const std::string& workspace_id, const std::string& slug) const;

    /** @brief Trigger a workflow run using string inputs. */
    WorkflowRun trigger(const std::string& workflow_id, const std::map<std::string, std::string>& inputs = {}) const;

    /**
     * Trigger a workflow run with pre-serialized JSON inputs.
     * inputs_json must be a JSON object string, e.g. "{\"prompt\":\"hello\"}".
     */
    WorkflowRun trigger_with_json(const std::string& workflow_id, const std::string& inputs_json) const;

private:
    std::reference_wrapper<const Client> client_;
};

/**
 * @brief Manager for workflow run lookup and cancellation.
 */
class WorkflowRunManager
{
public:
    /** @brief Construct a workflow run manager bound to a client. */
    explicit WorkflowRunManager(const Client& client);

    /** @brief List workflow runs, optionally filtered by workflow UUID and status. */
    std::vector<WorkflowRun> list(const std::string& workflow_id = "", const std::string& status_filter = "") const;

    /** @brief Fetch a workflow run by UUID. */
    WorkflowRun get(const std::string& run_id) const;

    /** @brief Cancel a workflow run by UUID. */
    WorkflowRun cancel(const std::string& run_id) const;

private:
    std::reference_wrapper<const Client> client_;
};

} // namespace cyberwave

#endif // CYBERWAVE_WORKFLOWS_H
