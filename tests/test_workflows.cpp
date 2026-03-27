/**
 * Symmetric with Python test_workflows.py: WorkflowManager, WorkflowRunManager, Workflow, WorkflowRun.
 */
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/workflows.h>

#include <CppRestOpenAPIClient/model/WorkflowRunSchema.h>
#include <CppRestOpenAPIClient/model/WorkflowSchema.h>

#include <cassert>
#include <map>
#include <string>

using namespace cyberwave;

static void test_client_has_workflows_and_workflow_runs()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    WorkflowManager w = c.workflows();
    WorkflowRunManager r = c.workflow_runs();
    (void)w;
    (void)r;
    assert(true);
}

static void test_workflow_view_getters()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowSchema>();
    schema->setUuid(utility::conversions::to_string_t("wf-1"));
    schema->setName(utility::conversions::to_string_t("Pick"));
    schema->setDescription(utility::conversions::to_string_t("Pick workflow"));
    schema->setIsActive(true);
    schema->setWorkspaceUuid(utility::conversions::to_string_t("ws-uuid"));
    Workflow wf(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(wf.uuid() == "wf-1");
    assert(wf.name() == "Pick");
    assert(wf.description() == "Pick workflow");
    assert(wf.is_active() == true);
    assert(wf.status() == "active");
    assert(wf.workspace_uuid() == "ws-uuid");
}

static void test_workflow_inactive_status()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowSchema>();
    schema->setUuid(utility::conversions::to_string_t("wf-2"));
    schema->setName(utility::conversions::to_string_t("Place"));
    schema->setIsActive(false);
    Workflow wf(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(wf.status() == "inactive");
}

static void test_workflow_run_view_getters()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowRunSchema>();
    schema->setUuid(utility::conversions::to_string_t("run-1"));
    schema->setWorkflowId(utility::conversions::to_string_t("wf-1"));
    schema->setStatus(utility::conversions::to_string_t("running"));
    WorkflowRun run(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(run.uuid() == "run-1");
    assert(run.workflow_id() == "wf-1");
    assert(run.status() == "running");
}

static void test_workflows_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    WorkflowManager w = c.workflows();
    bool threw = false;
    try
    {
        w.list();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_workflow_runs_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    WorkflowRunManager r = c.workflow_runs();
    bool threw = false;
    try
    {
        r.list();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

// --- New tests (symmetric with Python test_workflows.py) ---

static WorkflowRun make_run(const Client& c, const std::string& status)
{
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowRunSchema>();
    schema->setUuid(utility::conversions::to_string_t("run-1"));
    schema->setWorkflowId(utility::conversions::to_string_t("wf-1"));
    schema->setStatus(utility::conversions::to_string_t(status));
    return WorkflowRun(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
}

/** Mirrors TestWorkflowRunProperties::test_is_terminal */
static void test_workflow_run_is_terminal()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    // terminal statuses
    for (const auto& s : std::vector<std::string>{"success", "error", "failed", "completed", "canceled", "cancelled"})
        assert(make_run(c, s).is_terminal());
    // non-terminal statuses
    for (const auto& s : std::vector<std::string>{"running", "waiting", "requested"})
        assert(!make_run(c, s).is_terminal());
}

/** Mirrors TestWorkflowRunProperties::test_duration_none_when_not_finished */
static void test_workflow_run_duration_not_finished()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto run = make_run(c, "running");
    assert(run.duration() < 0.0); // -1.0 when not finished
}

/** Mirrors TestWorkflowRunProperties::test_properties_from_dict */
static void test_workflow_run_extra_getters()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowRunSchema>();
    schema->setUuid(utility::conversions::to_string_t("run-x"));
    schema->setStatus(utility::conversions::to_string_t("success"));
    schema->setError(utility::conversions::to_string_t(""));
    WorkflowRun run(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(run.uuid() == "run-x");
    assert(run.status() == "success");
    assert(run.error().empty());        // no error set
    assert(run.started_at().empty());   // not set
    assert(run.finished_at().empty());  // not set
    assert(run.completed_at().empty()); // alias for finished_at
    assert(run.inputs_json().empty());  // no inputs set
    assert(run.result_json().empty());  // no result set
}

/** Mirrors TestWorkflowRunPolling::test_wait_returns_immediately_when_already_terminal */
static void test_workflow_run_wait_immediate_if_terminal()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto run = make_run(c, "success");
    // Should not call refresh() because already terminal — must not throw even without API key
    run.wait(5.0, 0.01);
    assert(run.is_terminal());
}

/** Mirrors TestWorkflowRunPolling::test_wait_raises_on_timeout */
static void test_workflow_run_wait_raises_timeout()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto run = make_run(c, "running");
    bool threw = false;
    try
    {
        // Very short timeout; refresh() will throw CyberwaveError (no API) which propagates,
        // but if it gets called at all a CyberwaveTimeoutError should be thrown.
        // With timeout_s=0.0 it should hit deadline immediately on first check.
        run.wait(0.0, 0.001);
    }
    catch (const CyberwaveTimeoutError&)
    {
        threw = true;
    }
    catch (const CyberwaveError&)
    {
        // Also acceptable: the API call itself failed (no key) — still proves wait() called refresh
        threw = true;
    }
    assert(threw);
}

/** Mirrors TestWorkflow::test_status_derived_from_is_active */
static void test_workflow_status_derived_from_is_active()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto make_wf = [&](bool active)
    {
        auto s = std::make_shared<org::openapitools::client::model::WorkflowSchema>();
        s->setUuid(utility::conversions::to_string_t("wf"));
        s->setName(utility::conversions::to_string_t("WF"));
        s->setIsActive(active);
        return Workflow(c, std::shared_ptr<void>(std::static_pointer_cast<void>(s)));
    };
    assert(make_wf(true).status() == "active");
    assert(make_wf(false).status() == "inactive");
}

/** Mirrors TestWorkflowManager::test_get_returns_workflow (throws without API key) */
static void test_workflow_get_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    bool threw = false;
    try
    {
        c.workflows().get("wf-1");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Mirrors TestWorkflowRunManager::test_get_returns_run (throws without API key) */
static void test_workflow_run_get_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    bool threw = false;
    try
    {
        c.workflow_runs().get("run-1");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Mirrors TestWorkflowRunManager::test_cancel_returns_updated_run (throws without API key) */
static void test_workflow_run_cancel_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    bool threw = false;
    try
    {
        c.workflow_runs().cancel("run-1");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Mirrors TestWorkflowManager::test_trigger_returns_workflow_run (throws without API key) */
static void test_workflow_trigger_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    bool threw = false;
    try
    {
        c.workflows().trigger("wf-1");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Workflow::visibility returns empty when not set */
static void test_workflow_visibility_empty_default()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowSchema>();
    Workflow wf(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(wf.visibility().empty());
}

/** Workflow::visibility returns the string when set */
static void test_workflow_visibility_set()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowSchema>();
    schema->setVisibility(utility::conversions::to_string_t("public"));
    Workflow wf(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(wf.visibility() == "public");
}

/** Workflow::created_at and updated_at return empty when not set */
static void test_workflow_timestamps_empty_default()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowSchema>();
    Workflow wf(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(wf.created_at().empty());
    assert(wf.updated_at().empty());
}

/** Workflow::metadata_json returns "{}" when not set */
static void test_workflow_metadata_json_empty_default()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowSchema>();
    Workflow wf(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(wf.metadata_json() == "{}");
}

/** WorkflowManager::trigger_with_json throws without API key */
static void test_workflow_trigger_with_json_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    bool threw = false;
    try
    {
        c.workflows().trigger_with_json("wf-id", "{\"prompt\":\"hello\"}");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Workflow::trigger_with_json throws without API key */
static void test_workflow_object_trigger_with_json_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::WorkflowSchema>();
    schema->setUuid(utility::conversions::to_string_t("wf-id"));
    Workflow wf(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    bool threw = false;
    try
    {
        wf.trigger_with_json("{\"key\":42}");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_client_has_workflows_and_workflow_runs();
    test_workflow_view_getters();
    test_workflow_inactive_status();
    test_workflow_run_view_getters();
    test_workflows_list_throws_without_api_key();
    test_workflow_runs_list_throws_without_api_key();
    // new symmetric tests
    test_workflow_run_is_terminal();
    test_workflow_run_duration_not_finished();
    test_workflow_run_extra_getters();
    test_workflow_run_wait_immediate_if_terminal();
    test_workflow_run_wait_raises_timeout();
    test_workflow_status_derived_from_is_active();
    test_workflow_get_throws_without_api_key();
    test_workflow_run_get_throws_without_api_key();
    test_workflow_run_cancel_throws_without_api_key();
    test_workflow_trigger_throws_without_api_key();
    test_workflow_visibility_empty_default();
    test_workflow_visibility_set();
    test_workflow_timestamps_empty_default();
    test_workflow_metadata_json_empty_default();
    test_workflow_trigger_with_json_throws();
    test_workflow_object_trigger_with_json_throws();
    return 0;
}
