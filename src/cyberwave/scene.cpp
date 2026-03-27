#include "cyberwave/scene.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/environments.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twin.h"
#include "cyberwave/twins.h"

#include "CppRestOpenAPIClient/api/DefaultApi.h"

#include <cpprest/details/basic_types.h>
#include <pplx/pplxtasks.h>

namespace cyberwave
{

namespace
{

static org::openapitools::client::api::DefaultApi* scene_api(const Client& client)
{
    return ClientAccess::default_api(&client);
}

utility::string_t scene_from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

} // namespace

Scene::Scene(const Client& client, std::string environment_id)
    : client_(client), environment_id_(std::move(environment_id))
{
}

std::vector<Twin> Scene::get_twins() const { return client_.get().environments().get_twins(environment_id_); }

Twin Scene::add_twin(const std::string& asset_id, const std::string& name, const std::string& description,
                     const std::vector<double>& position, const std::vector<double>& orientation, bool fixed_base) const
{
    return client_.get().twins().create(asset_id, environment_id_, name, description, position, orientation,
                                        fixed_base);
}

Twin Scene::dock(const std::string& twin_id, const std::string& parent_twin_id, const std::string& link_name,
                 const std::vector<double>& offset_position, const std::vector<double>& offset_rotation) const
{
    return client_.get().twins().update(twin_id, "", "", parent_twin_id, link_name, offset_position, offset_rotation);
}

Twin Scene::undock(const std::string& twin_id) const { return client_.get().twins().update(twin_id, "", "", "", ""); }

std::string Scene::get_composed_schema() const
{
    auto* a = scene_api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        // The generated REST client endpoint returns void; the response body is not captured.
        // This call validates connectivity and auth. Use the Python SDK or direct HTTP for the full schema.
        a->srcAppApiEnvironmentsExportsGetEnvironmentUniversalSchemaJson(scene_from_std(environment_id_)).get();
        return "";
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(utility::conversions::to_utf8string(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
