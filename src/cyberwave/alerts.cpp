#include "cyberwave/alerts.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twin.h"

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/AlertSchema.h"
#include "CppRestOpenAPIClient/model/CreateAlertSchema.h"
#include "CppRestOpenAPIClient/model/UpdateAlertSchema.h"

#include <cpprest/details/basic_types.h>
#include <pplx/pplxtasks.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace cyberwave
{

namespace
{

using AlertSchemaPtr = std::shared_ptr<org::openapitools::client::model::AlertSchema>;
using AnyTypePtr = std::shared_ptr<org::openapitools::client::model::AnyType>;

std::string to_std(const utility::string_t& t)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return std::string(t);
#else
    return utility::conversions::to_utf8string(t);
#endif
}

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

AlertSchemaPtr get_schema(const std::shared_ptr<void>& p)
{
    return std::static_pointer_cast<org::openapitools::client::model::AlertSchema>(p);
}

std::string datetime_to_string(const utility::datetime& dt)
{
    return to_std(dt.to_string(utility::datetime::ISO_8601));
}

std::map<utility::string_t, AnyTypePtr> string_map_to_anytype_map(const std::map<std::string, std::string>& values)
{
    std::map<utility::string_t, AnyTypePtr> result;
    for (const auto& kv : values)
    {
        auto any_val = std::make_shared<org::openapitools::client::model::AnyType>();
        any_val->fromJson(web::json::value::string(from_std(kv.second)));
        result[from_std(kv.first)] = any_val;
    }
    return result;
}

std::map<std::string, std::string> anytype_map_to_string_map(const std::map<utility::string_t, AnyTypePtr>& values)
{
    std::map<std::string, std::string> result;
    for (const auto& kv : values)
    {
        if (!kv.second)
            continue;
        const web::json::value json = kv.second->toJson();
        result[to_std(kv.first)] = json.is_string() ? to_std(json.as_string()) : to_std(json.serialize());
    }
    return result;
}

} // namespace

Alert::Alert(const Client& client, std::shared_ptr<void> alert_schema_ptr)
    : client_(client), schema_(std::move(alert_schema_ptr))
{
}

std::string Alert::uuid() const
{
    auto s = get_schema(schema_);
    return s && s->uuidIsSet() ? to_std(s->getUuid()) : "";
}
std::string Alert::name() const
{
    auto s = get_schema(schema_);
    return s && s->nameIsSet() ? to_std(s->getName()) : "";
}
std::string Alert::description() const
{
    auto s = get_schema(schema_);
    return s && s->descriptionIsSet() ? to_std(s->getDescription()) : "";
}
std::string Alert::media() const
{
    auto s = get_schema(schema_);
    return s && s->mediaIsSet() ? to_std(s->getMedia()) : "";
}
std::string Alert::alert_type() const
{
    auto s = get_schema(schema_);
    return s && s->alertTypeIsSet() ? to_std(s->getAlertType()) : "";
}
std::string Alert::severity() const
{
    auto s = get_schema(schema_);
    return s && s->severityIsSet() ? to_std(s->getSeverity()) : "";
}
std::string Alert::status() const
{
    auto s = get_schema(schema_);
    return s && s->statusIsSet() ? to_std(s->getStatus()) : "";
}
std::string Alert::source_type() const
{
    auto s = get_schema(schema_);
    return s && s->sourceTypeIsSet() ? to_std(s->getSourceType()) : "";
}
std::string Alert::twin_uuid() const
{
    auto s = get_schema(schema_);
    return s && s->twinUuidIsSet() ? to_std(s->getTwinUuid()) : "";
}
std::string Alert::environment_uuid() const
{
    auto s = get_schema(schema_);
    return s && s->environmentUuidIsSet() ? to_std(s->getEnvironmentUuid()) : "";
}
std::string Alert::workflow_uuid() const
{
    auto s = get_schema(schema_);
    return s && s->workflowUuidIsSet() ? to_std(s->getWorkflowUuid()) : "";
}
std::string Alert::workspace_uuid() const
{
    auto s = get_schema(schema_);
    return s && s->workspaceUuidIsSet() ? to_std(s->getWorkspaceUuid()) : "";
}
std::string Alert::created_at() const
{
    auto s = get_schema(schema_);
    return s && s->createdAtIsSet() ? datetime_to_string(s->getCreatedAt()) : "";
}
std::string Alert::updated_at() const
{
    auto s = get_schema(schema_);
    return s && s->updatedAtIsSet() ? datetime_to_string(s->getUpdatedAt()) : "";
}
std::string Alert::resolved_at() const
{
    auto s = get_schema(schema_);
    return s && s->resolvedAtIsSet() ? datetime_to_string(s->getResolvedAt()) : "";
}
std::map<std::string, std::string> Alert::metadata() const
{
    auto s = get_schema(schema_);
    if (!s || !s->metadataIsSet())
        return {};
    return anytype_map_to_string_map(s->getMetadata());
}

static org::openapitools::client::api::DefaultApi* api(const Client& client)
{
    return ClientAccess::default_api(&client);
}

Alert& Alert::acknowledge()
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiAlertsAcknowledgeAlert(from_std(uuid())).get();
        if (result)
            schema_ = std::shared_ptr<void>(std::static_pointer_cast<void>(result));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
    return *this;
}

Alert& Alert::resolve()
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiAlertsResolveAlert(from_std(uuid())).get();
        if (result)
            schema_ = std::shared_ptr<void>(std::static_pointer_cast<void>(result));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
    return *this;
}

Alert& Alert::silence()
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiAlertsSilenceAlert(from_std(uuid())).get();
        if (result)
            schema_ = std::shared_ptr<void>(std::static_pointer_cast<void>(result));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
    return *this;
}

Alert& Alert::press_button(int button_index)
{
    if (button_index < 0)
        throw CyberwaveError("button_index must be >= 0");
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiAlertsPressAlertButton(from_std(uuid()), button_index).get();
        if (result)
            schema_ = std::shared_ptr<void>(std::static_pointer_cast<void>(result));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
    return *this;
}

Alert& Alert::refresh()
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiAlertsGetAlert(from_std(uuid())).get();
        if (result)
            schema_ = std::shared_ptr<void>(std::static_pointer_cast<void>(result));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
    return *this;
}

void Alert::delete_alert()
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        a->srcAppApiAlertsDeleteAlert(from_std(uuid())).get();
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

TwinAlertManager::TwinAlertManager(std::shared_ptr<const Twin> twin) : twin_(std::move(twin))
{
    if (!twin_)
        throw std::invalid_argument("TwinAlertManager requires a non-null twin");
}

Alert TwinAlertManager::create(const std::string& name) { return create(name, CreateOptions()); }

Alert TwinAlertManager::create(const std::string& name, const CreateOptions& options)
{
    const auto& client = twin_->client();
    auto* a = api(client);
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    auto body = std::make_shared<org::openapitools::client::model::CreateAlertSchema>();
    body->setName(from_std(name));
    body->setDescription(from_std(options.description));
    if (!options.media.empty())
        body->setMedia(from_std(options.media));
    body->setAlertType(from_std(options.alert_type));
    body->setSeverity(from_std(options.severity));
    body->setSourceType(from_std(options.source_type));
    body->setTwinUuid(from_std(twin_->uuid()));
    if (!options.workspace_uuid.empty())
        body->setWorkspaceUuid(from_std(options.workspace_uuid));
    if (!options.environment_uuid.empty())
        body->setEnvironmentUuid(from_std(options.environment_uuid));
    if (!options.workflow_uuid.empty())
        body->setWorkflowUuid(from_std(options.workflow_uuid));
    if (!options.metadata.empty())
        body->setMetadata(string_map_to_anytype_map(options.metadata));
    if (options.force)
        body->setForce(true);
    try
    {
        auto result = a->srcAppApiAlertsCreateAlert(body).get();
        if (result)
            return Alert(client, std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
    throw CyberwaveError("Create alert returned no data");
}

Alert TwinAlertManager::get(const std::string& uuid) const
{
    const auto& client = twin_->client();
    auto* a = api(client);
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiAlertsGetAlert(from_std(uuid)).get();
        if (result)
            return Alert(client, std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
    throw CyberwaveError("Get alert returned no data");
}

std::vector<Alert> TwinAlertManager::list() const { return list(ListOptions()); }

std::vector<Alert> TwinAlertManager::list(const ListOptions& options) const
{
    const auto& client = twin_->client();
    auto* a = api(client);
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    boost::optional<utility::string_t> st, sev;
    boost::optional<int32_t> limit(options.limit);
    if (!options.status.empty())
        st = from_std(options.status);
    if (!options.severity.empty())
        sev = from_std(options.severity);
    try
    {
        auto vec = a->srcAppApiAlertsListAlerts(boost::optional<utility::string_t>(),
                                                boost::optional<utility::string_t>(from_std(twin_->uuid())),
                                                boost::optional<utility::string_t>(), st, sev, limit)
                       .get();
        std::vector<Alert> out;
        for (auto& ptr : vec)
        {
            if (ptr)
                out.push_back(Alert(client, std::shared_ptr<void>(std::static_pointer_cast<void>(ptr))));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Alert& Alert::update(const UpdateOptions& opts)
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    auto body = std::make_shared<org::openapitools::client::model::UpdateAlertSchema>();
    if (opts.name)
        body->setName(from_std(*opts.name));
    if (opts.description)
        body->setDescription(from_std(*opts.description));
    if (opts.media)
        body->setMedia(from_std(*opts.media));
    if (opts.alert_type)
        body->setAlertType(from_std(*opts.alert_type));
    if (opts.severity)
        body->setSeverity(from_std(*opts.severity));
    if (opts.status)
        body->setStatus(from_std(*opts.status));
    if (opts.metadata)
        body->setMetadata(string_map_to_anytype_map(*opts.metadata));
    try
    {
        auto result = a->srcAppApiAlertsUpdateAlert(from_std(uuid()), body).get();
        if (result)
            schema_ = std::shared_ptr<void>(std::static_pointer_cast<void>(result));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
    return *this;
}

} // namespace cyberwave
