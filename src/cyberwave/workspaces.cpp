#include "cyberwave/workspaces.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"

#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/WorkspaceCreateSchema.h"
#include "CppRestOpenAPIClient/model/WorkspaceResponseSchema.h"
#include "CppRestOpenAPIClient/model/WorkspaceSchema.h"
#include "CppRestOpenAPIClient/model/WorkspaceUpdateSchema.h"

#include <boost/optional.hpp>
#include <cpprest/details/basic_types.h>
#include <pplx/pplxtasks.h>

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

Workspace::Workspace(std::shared_ptr<void> schema_ptr) : schema_(std::move(schema_ptr)) {}

std::string Workspace::uuid() const
{
    auto ws = std::static_pointer_cast<org::openapitools::client::model::WorkspaceSchema>(schema_);
    return ws && ws->uuidIsSet() ? to_std(ws->getUuid()) : "";
}

std::string Workspace::name() const
{
    auto ws = std::static_pointer_cast<org::openapitools::client::model::WorkspaceSchema>(schema_);
    return ws && ws->nameIsSet() ? to_std(ws->getName()) : "";
}

std::string Workspace::description() const
{
    auto ws = std::static_pointer_cast<org::openapitools::client::model::WorkspaceSchema>(schema_);
    return ws && ws->descriptionIsSet() ? to_std(ws->getDescription()) : "";
}

std::string Workspace::slug() const
{
    auto ws = std::static_pointer_cast<org::openapitools::client::model::WorkspaceSchema>(schema_);
    return ws && ws->slugIsSet() ? to_std(ws->getSlug()) : "";
}

WorkspaceManager::WorkspaceManager(const Client& client) : client_(client) {}

std::vector<Workspace> WorkspaceManager::list() const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto vec = a->srcUsersApiWorkspacesListWorkspaces().get();
        std::vector<Workspace> out;
        for (auto& ptr : vec)
        {
            if (ptr)
                out.push_back(Workspace(std::shared_ptr<void>(std::static_pointer_cast<void>(ptr))));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Workspace WorkspaceManager::get(const std::string& workspace_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto resp = a->srcUsersApiWorkspacesGetWorkspace(from_std(workspace_id)).get();
        if (!resp)
            throw CyberwaveError("Get workspace returned no data");
        auto team = resp->getTeam();
        if (!team)
            return Workspace(std::shared_ptr<void>(
                std::static_pointer_cast<void>(std::make_shared<org::openapitools::client::model::WorkspaceSchema>())));
        return Workspace(std::shared_ptr<void>(std::static_pointer_cast<void>(team)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Workspace WorkspaceManager::create(const std::string& name, const std::string& description) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::WorkspaceCreateSchema>();
        body->setName(from_std(name));
        if (!description.empty())
            body->setDescription(from_std(description));
        auto result =
            a->srcUsersApiWorkspacesCreateWorkspace(
                 boost::optional<std::shared_ptr<org::openapitools::client::model::WorkspaceCreateSchema>>(body))
                .get();
        if (!result)
            throw CyberwaveError("Create workspace returned no data");
        return Workspace(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Workspace WorkspaceManager::update(const std::string& workspace_id, const std::string& name,
                                   const std::string& description, const std::string& slug) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::WorkspaceUpdateSchema>();
        if (!name.empty())
            body->setName(from_std(name));
        if (!description.empty())
            body->setDescription(from_std(description));
        if (!slug.empty())
            body->setSlug(from_std(slug));
        auto result = a->srcUsersApiWorkspacesUpdateWorkspace(from_std(workspace_id), body).get();
        if (!result)
            throw CyberwaveError("Update workspace returned no data");
        return Workspace(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
