#include "cyberwave/environments.h"

#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twin.h"
#include <optional>

#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/EnvironmentCreateSchema.h"
#include "CppRestOpenAPIClient/model/EnvironmentSchema.h"
#include "CppRestOpenAPIClient/model/TwinSchema.h"

#include <cpprest/details/basic_types.h>
#include <pplx/pplxtasks.h>

#include <algorithm>

namespace cyberwave
{

namespace
{

std::string to_std(const utility::string_t& t)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return std::string(t);
#else
    return utility::conversions::to_utf8string(t);
#endif
}

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

static org::openapitools::client::api::DefaultApi* api(const Client& client)
{
    return ClientAccess::default_api(&client);
}

} // namespace

Environment::Environment(std::shared_ptr<void> schema_ptr) : schema_(std::move(schema_ptr)) {}

std::string Environment::uuid() const
{
    auto es = std::static_pointer_cast<org::openapitools::client::model::EnvironmentSchema>(schema_);
    return es && es->uuidIsSet() ? to_std(es->getUuid()) : "";
}

std::string Environment::name() const
{
    auto es = std::static_pointer_cast<org::openapitools::client::model::EnvironmentSchema>(schema_);
    return es && es->nameIsSet() ? to_std(es->getName()) : "";
}

std::string Environment::description() const
{
    auto es = std::static_pointer_cast<org::openapitools::client::model::EnvironmentSchema>(schema_);
    return es && es->descriptionIsSet() ? to_std(es->getDescription()) : "";
}

std::string Environment::project_uuid() const
{
    auto es = std::static_pointer_cast<org::openapitools::client::model::EnvironmentSchema>(schema_);
    return es && es->projectUuidIsSet() ? to_std(es->getProjectUuid()) : "";
}

std::string Environment::workspace_uuid() const
{
    auto es = std::static_pointer_cast<org::openapitools::client::model::EnvironmentSchema>(schema_);
    return es && es->workspaceUuidIsSet() ? to_std(es->getWorkspaceUuid()) : "";
}

EnvironmentManager::EnvironmentManager(const Client& client) : client_(client) {}

std::vector<Environment> EnvironmentManager::list(const std::string& project_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        std::vector<std::shared_ptr<org::openapitools::client::model::EnvironmentSchema>> vec;
        if (!project_id.empty())
        {
            vec = a->srcAppApiEnvironmentsListEnvironmentsForProject(from_std(project_id)).get();
        }
        else
        {
            vec = a->srcAppApiEnvironmentsListAllEnvironments().get();
        }
        std::vector<Environment> out;
        for (auto& ptr : vec)
        {
            if (ptr)
                out.push_back(Environment(std::shared_ptr<void>(std::static_pointer_cast<void>(ptr))));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::optional<Environment> EnvironmentManager::get_first_or_none() const
{
    auto envs = list();
    if (envs.empty())
        return std::nullopt;
    return envs.front();
}

Environment EnvironmentManager::get(const std::string& environment_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiEnvironmentsGetEnvironment(from_std(environment_id)).get();
        if (!result)
            throw CyberwaveError("Get environment returned no data");
        return Environment(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Environment EnvironmentManager::create(const std::string& name, const std::string& project_id,
                                       const std::string& description) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        utility::string_t proj_uuid;
        if (!project_id.empty())
        {
            proj_uuid = from_std(project_id);
        }
        else
        {
            auto projects = a->srcAppApiProjectsListProjects().get();
            if (projects.empty())
                throw CyberwaveError("create environment requires project_id or at least one existing project");
            proj_uuid = projects[0]->getUuid();
        }
        auto body = std::make_shared<org::openapitools::client::model::EnvironmentCreateSchema>();
        body->setName(from_std(name));
        if (!description.empty())
            body->setDescription(from_std(description));
        auto result = a->srcAppApiEnvironmentsCreateEnvironmentForProject(proj_uuid, body).get();
        if (!result)
            throw CyberwaveError("Create environment returned no data");
        return Environment(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

void EnvironmentManager::delete_environment(const std::string& environment_id, const std::string& project_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        a->srcAppApiEnvironmentsDeleteEnvironmentForProject(from_std(project_id), from_std(environment_id)).get();
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::vector<Twin> EnvironmentManager::get_twins(const std::string& environment_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto vec = a->srcAppApiEnvironmentsGetEnvironmentTwins(from_std(environment_id)).get();
        std::vector<Twin> out;
        for (auto& ptr : vec)
        {
            if (ptr)
                out.push_back(Twin(client_, ptr->uuidIsSet() ? to_std(ptr->getUuid()) : "",
                                   ptr->nameIsSet() ? to_std(ptr->getName()) : ""));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::string EnvironmentManager::get_universal_schema_json(const std::string& environment_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        // Generated client returns void; body not capturable. Calls endpoint for auth/connectivity check.
        a->srcAppApiEnvironmentsExportsGetEnvironmentUniversalSchemaJson(from_std(environment_id)).get();
        return "";
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::vector<unsigned char> EnvironmentManager::export_urdf_scene(const std::string& environment_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        // Generated client returns void; body not capturable.
        a->srcAppApiEnvironmentsExportsGetEnvironmentUrdfScene(from_std(environment_id)).get();
        return {};
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::vector<unsigned char> EnvironmentManager::export_mujoco_scene(const std::string& environment_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        // Generated client returns void; body not capturable.
        a->srcAppApiEnvironmentsExportsGetEnvironmentMujocoScene(from_std(environment_id)).get();
        return {};
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
