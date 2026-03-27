#include "cyberwave/edges.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/EdgeCreateSchema.h"
#include "CppRestOpenAPIClient/model/EdgeSchema.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>
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

Edge::Edge(std::shared_ptr<void> schema_ptr) : schema_(std::move(schema_ptr)) {}

std::string Edge::uuid() const
{
    auto es = std::static_pointer_cast<org::openapitools::client::model::EdgeSchema>(schema_);
    return es && es->uuidIsSet() ? to_std(es->getUuid()) : "";
}

std::string Edge::name() const
{
    auto es = std::static_pointer_cast<org::openapitools::client::model::EdgeSchema>(schema_);
    return es && es->nameIsSet() ? to_std(es->getName()) : "";
}

std::string Edge::fingerprint() const
{
    auto es = std::static_pointer_cast<org::openapitools::client::model::EdgeSchema>(schema_);
    return es && es->fingerprintIsSet() ? to_std(es->getFingerprint()) : "";
}

EdgeManager::EdgeManager(const Client& client) : client_(client) {}

std::vector<Edge> EdgeManager::list() const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto vec = a->srcAppApiEdgesGetEdges().get();
        std::vector<Edge> out;
        for (auto& ptr : vec)
        {
            if (ptr)
                out.push_back(Edge(std::shared_ptr<void>(std::static_pointer_cast<void>(ptr))));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Edge EdgeManager::get(const std::string& edge_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiEdgesGetEdge(from_std(edge_id)).get();
        if (!result)
            throw CyberwaveError("Get edge returned no data");
        return Edge(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

namespace
{
void set_edge_metadata(org::openapitools::client::model::EdgeCreateSchema& body,
                       const std::map<std::string, std::string>& metadata)
{
    if (metadata.empty())
        return;
    std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>> m;
    for (const auto& kv : metadata)
    {
        auto any_val = std::make_shared<org::openapitools::client::model::AnyType>();
        any_val->fromJson(web::json::value::string(utility::conversions::to_string_t(kv.second)));
        m[utility::conversions::to_string_t(kv.first)] = any_val;
    }
    body.setMetadata(m);
}
} // namespace

Edge EdgeManager::create(const std::string& fingerprint, const std::string& name, const std::string& workspace_id,
                         const std::map<std::string, std::string>& metadata) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::EdgeCreateSchema>();
        body->setFingerprint(from_std(fingerprint));
        if (!name.empty())
            body->setName(from_std(name));
        if (!workspace_id.empty())
            body->setWorkspaceUuid(from_std(workspace_id));
        set_edge_metadata(*body, metadata);
        auto result = a->srcAppApiEdgesCreateEdge(body).get();
        if (!result)
            throw CyberwaveError("Create edge returned no data");
        return Edge(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Edge EdgeManager::update(const std::string& edge_id, const std::string& name,
                         const std::map<std::string, std::string>& metadata) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::EdgeCreateSchema>();
        if (!name.empty())
            body->setName(from_std(name));
        set_edge_metadata(*body, metadata);
        auto result = a->srcAppApiEdgesUpdateEdge(from_std(edge_id), body).get();
        if (!result)
            throw CyberwaveError("Update edge returned no data");
        return Edge(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

void EdgeManager::delete_edge(const std::string& edge_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        a->srcAppApiEdgesDeleteEdge(from_std(edge_id)).get();
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
