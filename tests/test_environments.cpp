/**
 * Symmetric with Python: client.environments, EnvironmentManager, Environment view.
 */
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/environments.h>
#include <cyberwave/exceptions.h>

#include <CppRestOpenAPIClient/model/EnvironmentSchema.h>

#include <cassert>
#include <string>

using namespace cyberwave;

static void test_client_has_environments()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    (void)e;
    assert(true);
}

static void test_environment_view_getters()
{
    auto schema = std::make_shared<org::openapitools::client::model::EnvironmentSchema>();
    schema->setUuid(utility::conversions::to_string_t("env-uuid"));
    schema->setName(utility::conversions::to_string_t("My Env"));
    schema->setDescription(utility::conversions::to_string_t("Description"));
    schema->setProjectUuid(utility::conversions::to_string_t("proj-uuid"));
    schema->setWorkspaceUuid(utility::conversions::to_string_t("ws-uuid"));
    Environment env(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(env.uuid() == "env-uuid");
    assert(env.name() == "My Env");
    assert(env.description() == "Description");
    assert(env.project_uuid() == "proj-uuid");
    assert(env.workspace_uuid() == "ws-uuid");
}

static void test_environments_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
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

/** get_first_or_none throws without api key (no backend available) */
static void test_environments_get_first_or_none_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        auto result = e.get_first_or_none();
        (void)result;
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** get_universal_schema_json throws without api key */
static void test_environments_get_universal_schema_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.get_universal_schema_json("some-env-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** export_urdf_scene throws without api key */
static void test_environments_export_urdf_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.export_urdf_scene("some-env-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** export_mujoco_scene throws without api key */
static void test_environments_export_mujoco_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.export_mujoco_scene("some-env-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_client_has_environments();
    test_environment_view_getters();
    test_environments_list_throws_without_api_key();
    test_environments_get_first_or_none_throws_without_api_key();
    test_environments_get_universal_schema_throws_without_api_key();
    test_environments_export_urdf_throws_without_api_key();
    test_environments_export_mujoco_throws_without_api_key();
    return 0;
}
