/**
 * Symmetric with Python: client.workspaces, WorkspaceManager, Workspace view.
 */
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/workspaces.h>

#include <CppRestOpenAPIClient/model/WorkspaceSchema.h>

#include <cassert>
#include <string>

using namespace cyberwave;

static void test_client_has_workspaces()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    WorkspaceManager w = c.workspaces();
    (void)w;
    assert(true);
}

static void test_workspace_view_getters()
{
    auto schema = std::make_shared<org::openapitools::client::model::WorkspaceSchema>();
    schema->setUuid(utility::conversions::to_string_t("ws-uuid"));
    schema->setName(utility::conversions::to_string_t("My Workspace"));
    schema->setDescription(utility::conversions::to_string_t("Description"));
    schema->setSlug(utility::conversions::to_string_t("my-workspace"));
    Workspace ws(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(ws.uuid() == "ws-uuid");
    assert(ws.name() == "My Workspace");
    assert(ws.description() == "Description");
    assert(ws.slug() == "my-workspace");
}

static void test_workspaces_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    WorkspaceManager w = c.workspaces();
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

int main()
{
    test_client_has_workspaces();
    test_workspace_view_getters();
    test_workspaces_list_throws_without_api_key();
    return 0;
}
