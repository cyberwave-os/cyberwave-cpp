#include "cyberwave/environments.h"

#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/rest_helpers.h"
#include "cyberwave/twin.h"
#include <optional>

#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/AttachmentSchema.h"
#include "CppRestOpenAPIClient/model/EnvironmentCreateSchema.h"
#include "CppRestOpenAPIClient/model/EnvironmentSchema.h"
#include "CppRestOpenAPIClient/model/EnvironmentUniversalSchemaPatchSchema.h"
#include "CppRestOpenAPIClient/model/TwinSchema.h"

#include <boost/optional.hpp>
#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>
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

static std::string
any_map_to_json(const std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>& values)
{
    web::json::value obj = web::json::value::object();
    for (const auto& [key, value] : values)
    {
        if (value)
        {
            obj[key] = value->toJson();
        }
    }
    return to_std(obj.serialize());
}

} // namespace

Attachment::Attachment(std::shared_ptr<void> schema_ptr) : schema_(std::move(schema_ptr)) {}

std::string Attachment::uuid() const
{
    auto attachment = std::static_pointer_cast<org::openapitools::client::model::AttachmentSchema>(schema_);
    return attachment && attachment->uuidIsSet() ? to_std(attachment->getUuid()) : "";
}

std::string Attachment::file_url() const
{
    auto attachment = std::static_pointer_cast<org::openapitools::client::model::AttachmentSchema>(schema_);
    return attachment && attachment->fileUrlIsSet() ? to_std(attachment->getFileUrl()) : "";
}

std::string Attachment::metadata_json() const
{
    auto attachment = std::static_pointer_cast<org::openapitools::client::model::AttachmentSchema>(schema_);
    if (!attachment || !attachment->metadataIsSet())
    {
        return "{}";
    }
    return any_map_to_json(attachment->getMetadata());
}

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
        constexpr int page_limit = 200;
        int offset = 0;
        std::vector<std::shared_ptr<org::openapitools::client::model::EnvironmentSchema>> vec;
        while (true)
        {
            std::vector<std::shared_ptr<org::openapitools::client::model::EnvironmentSchema>> page;
            if (!project_id.empty())
            {
                page =
                    a->srcAppApiEnvironmentsListEnvironmentsForProject(from_std(project_id), page_limit, offset).get();
            }
            else
            {
                page = a->srcAppApiEnvironmentsListAllEnvironments(page_limit, offset).get();
            }
            if (page.empty())
            {
                break;
            }
            vec.insert(vec.end(), page.begin(), page.end());
            if (static_cast<int>(page.size()) < page_limit)
            {
                break;
            }
            offset += page_limit;
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
    auto response = detail::request_raw(client_.get(),
                                        from_std("/api/v1/environments/" + environment_id + "/universal-schema.json"),
                                        web::http::methods::GET);
    return response.text();
}

std::vector<unsigned char> EnvironmentManager::export_urdf_scene(const std::string& environment_id) const
{
    auto response = detail::request_raw(
        client_.get(), from_std("/api/v1/environments/" + environment_id + "/urdf-scene.zip"), web::http::methods::GET);
    return response.body;
}

std::vector<unsigned char> EnvironmentManager::export_mujoco_scene(const std::string& environment_id) const
{
    auto response =
        detail::request_raw(client_.get(), from_std("/api/v1/environments/" + environment_id + "/mujoco-scene.zip"),
                            web::http::methods::GET);
    return response.body;
}

Attachment EnvironmentManager::create_preview(const std::string& environment_id) const
{
    try
    {
        auto response = detail::request_raw(
            client_.get(), from_std("/api/v1/environments/" + environment_id + "/preview"), web::http::methods::POST);
        auto attachment = std::make_shared<org::openapitools::client::model::AttachmentSchema>();
        if (!attachment->fromJson(detail::parse_json_response(response)))
            throw CyberwaveError("Generate environment preview returned invalid attachment data");
        return Attachment(std::shared_ptr<void>(std::static_pointer_cast<void>(attachment)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::string EnvironmentManager::set_universal_schema(const std::string& environment_id,
                                                     const std::string& schema_json) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto current = a->srcAppApiEnvironmentsGetEnvironment(from_std(environment_id)).get();
        if (!current)
            throw CyberwaveError("Get environment returned no data");

        auto body = std::make_shared<org::openapitools::client::model::EnvironmentCreateSchema>();
        if (current->nameIsSet())
            body->setName(current->getName());
        if (current->descriptionIsSet())
            body->setDescription(current->getDescription());
        if (current->settingsIsSet())
            body->setSettings(current->getSettings());
        if (current->projectUuidIsSet())
            body->setProjectUuid(current->getProjectUuid());
        if (current->workspaceUuidIsSet())
            body->setWorkspaceUuid(current->getWorkspaceUuid());
        if (current->visibilityIsSet())
            body->setVisibility(current->getVisibility());

        const web::json::value parsed_schema = web::json::value::parse(from_std(schema_json));
        body->setUniversalSchema(detail::json_object_to_any_map(parsed_schema));

        auto result = a->srcAppApiEnvironmentsUpdateEnvironment(from_std(environment_id), body).get();
        if (!result)
            throw CyberwaveError("Update environment returned no data");
        return any_map_to_json(result->getUniversalSchema());
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::string EnvironmentManager::patch_universal_schema(const std::string& environment_id, const std::string& path,
                                                       const std::string& value_json, const std::string& op) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::EnvironmentUniversalSchemaPatchSchema>();
        body->setOp(from_std(op));
        body->setPath(from_std(path));

        auto any_value = std::make_shared<org::openapitools::client::model::AnyType>();
        any_value->fromJson(web::json::value::parse(from_std(value_json)));
        body->setValue(any_value);

        auto result = a->srcAppApiEnvironmentsPatchEnvironmentUniversalSchema(from_std(environment_id), body).get();
        return any_map_to_json(result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
