#include "cyberwave/projects.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"

#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/ProjectCreateSchema.h"
#include "CppRestOpenAPIClient/model/ProjectSchema.h"

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

Project::Project(std::shared_ptr<void> schema_ptr) : schema_(std::move(schema_ptr)) {}

std::string Project::uuid() const
{
    auto ps = std::static_pointer_cast<org::openapitools::client::model::ProjectSchema>(schema_);
    return ps && ps->uuidIsSet() ? to_std(ps->getUuid()) : "";
}

std::string Project::name() const
{
    auto ps = std::static_pointer_cast<org::openapitools::client::model::ProjectSchema>(schema_);
    return ps && ps->nameIsSet() ? to_std(ps->getName()) : "";
}

std::string Project::description() const
{
    auto ps = std::static_pointer_cast<org::openapitools::client::model::ProjectSchema>(schema_);
    return ps && ps->descriptionIsSet() ? to_std(ps->getDescription()) : "";
}

std::string Project::workspace_uuid() const
{
    auto ps = std::static_pointer_cast<org::openapitools::client::model::ProjectSchema>(schema_);
    return ps && ps->workspaceUuidIsSet() ? to_std(ps->getWorkspaceUuid()) : "";
}

ProjectManager::ProjectManager(const Client& client) : client_(client) {}

std::vector<Project> ProjectManager::list(const std::string& workspace_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto vec = a->srcAppApiProjectsListProjects().get();
        std::vector<Project> out;
        for (auto& ptr : vec)
        {
            if (!ptr)
                continue;
            if (!workspace_id.empty())
            {
                if (ptr->workspaceUuidIsSet() && to_std(ptr->getWorkspaceUuid()) != workspace_id)
                    continue;
            }
            out.push_back(Project(std::shared_ptr<void>(std::static_pointer_cast<void>(ptr))));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Project ProjectManager::get(const std::string& project_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiProjectsGetProject(from_std(project_id)).get();
        if (!result)
            throw CyberwaveError("Get project returned no data");
        return Project(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Project ProjectManager::create(const std::string& name, const std::string& workspace_id,
                               const std::string& description) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::ProjectCreateSchema>();
        body->setName(from_std(name));
        if (!workspace_id.empty())
            body->setWorkspaceUuid(from_std(workspace_id));
        if (!description.empty())
            body->setDescription(from_std(description));
        auto result = a->srcAppApiProjectsCreateProject(body).get();
        if (!result)
            throw CyberwaveError("Create project returned no data");
        return Project(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Project ProjectManager::update(const std::string& project_id, const std::string& name,
                               const std::string& description) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::ProjectCreateSchema>();
        if (!name.empty())
            body->setName(from_std(name));
        if (!description.empty())
            body->setDescription(from_std(description));
        auto result = a->srcAppApiProjectsUpdateProject(from_std(project_id), body).get();
        if (!result)
            throw CyberwaveError("Update project returned no data");
        return Project(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

void ProjectManager::delete_project(const std::string& project_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        a->srcAppApiProjectsDeleteProject(from_std(project_id)).get();
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
