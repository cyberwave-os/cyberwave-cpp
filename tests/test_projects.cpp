/**
 * Symmetric with Python: client.projects, ProjectManager, Project view.
 */
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/projects.h>

#include <CppRestOpenAPIClient/model/ProjectSchema.h>

#include <cassert>
#include <string>

using namespace cyberwave;

static void test_client_has_projects()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    ProjectManager p = c.projects();
    (void)p;
    assert(true);
}

static void test_project_view_getters()
{
    auto schema = std::make_shared<org::openapitools::client::model::ProjectSchema>();
    schema->setUuid(utility::conversions::to_string_t("proj-uuid"));
    schema->setName(utility::conversions::to_string_t("My Project"));
    schema->setDescription(utility::conversions::to_string_t("Description"));
    schema->setWorkspaceUuid(utility::conversions::to_string_t("ws-uuid"));
    Project proj(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(proj.uuid() == "proj-uuid");
    assert(proj.name() == "My Project");
    assert(proj.description() == "Description");
    assert(proj.workspace_uuid() == "ws-uuid");
}

static void test_projects_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    ProjectManager p = c.projects();
    bool threw = false;
    try
    {
        p.list();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_client_has_projects();
    test_project_view_getters();
    test_projects_list_throws_without_api_key();
    return 0;
}
