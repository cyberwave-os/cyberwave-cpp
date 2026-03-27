/**
 * Symmetric with Python test_edges.py: client.edges, EdgeManager, Edge view.
 */
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/edges.h>
#include <cyberwave/exceptions.h>

#include <CppRestOpenAPIClient/model/EdgeSchema.h>

#include <cyberwave/alerts.h>
#include <cyberwave/assets.h>
#include <cyberwave/environments.h>
#include <cyberwave/projects.h>
#include <cyberwave/twins.h>
#include <cyberwave/workflows.h>
#include <cyberwave/workspaces.h>

#include <cassert>
#include <string>

using namespace cyberwave;

static void test_client_has_edges()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EdgeManager e = c.edges();
    (void)e;
    assert(true);
}

static void test_edge_view_getters()
{
    auto schema = std::make_shared<org::openapitools::client::model::EdgeSchema>();
    schema->setUuid(utility::conversions::to_string_t("edge-uuid-1"));
    schema->setName(utility::conversions::to_string_t("Edge Device"));
    schema->setFingerprint(utility::conversions::to_string_t("fp-abc"));
    Edge ed(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(ed.uuid() == "edge-uuid-1");
    assert(ed.name() == "Edge Device");
    assert(ed.fingerprint() == "fp-abc");
}

static void test_edges_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EdgeManager e = c.edges();
    bool threw = false;
    try
    {
        e.list();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Mirrors test_client_has_all_expected_managers (test_edges.py) */
static void test_client_has_all_expected_managers()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    // Access all managers — must not throw on construction
    WorkspaceManager wm = c.workspaces();
    (void)wm;
    ProjectManager pm = c.projects();
    (void)pm;
    EnvironmentManager em = c.environments();
    (void)em;
    AssetManager am = c.assets();
    (void)am;
    EdgeManager edm = c.edges();
    (void)edm;
    TwinManager tm = c.twins();
    (void)tm;
    WorkflowManager wfm = c.workflows();
    (void)wfm;
    WorkflowRunManager rm = c.workflow_runs();
    (void)rm;
}

/** Mirrors test_edge_manager_create_raises_on_api_error */
static void test_edge_manager_create_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EdgeManager e = c.edges();
    bool threw = false;
    try
    {
        e.create("fp-abc", "My Edge");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Mirrors test_edge_manager_update_sends_data */
static void test_edge_manager_update_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EdgeManager e = c.edges();
    bool threw = false;
    try
    {
        e.update("edge-uuid-1", "Updated Name");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Mirrors test_edge_manager_delete_calls_api */
static void test_edge_manager_delete_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EdgeManager e = c.edges();
    bool threw = false;
    try
    {
        e.delete_edge("edge-uuid-1");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Mirrors test_edge_manager_get_returns_edge (throws without API key) */
static void test_edge_manager_get_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EdgeManager e = c.edges();
    bool threw = false;
    try
    {
        e.get("edge-uuid-1");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** create with metadata param (throws without API key) */
static void test_edge_manager_create_with_metadata_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EdgeManager e = c.edges();
    bool threw = false;
    try
    {
        e.create("fp-xyz", "My Edge", "ws-1", {{"env", "prod"}, {"region", "us-east"}});
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** update with metadata param (throws without API key) */
static void test_edge_manager_update_with_metadata_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EdgeManager e = c.edges();
    bool threw = false;
    try
    {
        e.update("edge-uuid-1", "New Name", {{"version", "2"}});
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_client_has_edges();
    test_edge_view_getters();
    test_edges_list_throws_without_api_key();
    // new symmetric tests
    test_client_has_all_expected_managers();
    test_edge_manager_create_throws_without_api_key();
    test_edge_manager_update_throws_without_api_key();
    test_edge_manager_delete_throws_without_api_key();
    test_edge_manager_get_throws_without_api_key();
    test_edge_manager_create_with_metadata_throws();
    test_edge_manager_update_with_metadata_throws();
    return 0;
}
