#include "cyberwave/joints.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twin.h"

#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/JointStateUpdateSchema.h"
#include "CppRestOpenAPIClient/model/JointStatesSchema.h"

#include <cpprest/details/basic_types.h>
#include <pplx/pplxtasks.h>

#include <cmath>
#include <string>

namespace cyberwave
{

namespace
{

org::openapitools::client::api::DefaultApi* api(const Client& client) { return ClientAccess::default_api(&client); }

std::string to_std(const utility::string_t& t)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return std::string(t);
#else
    return utility::conversions::to_utf8string(t);
#endif
}

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

} // namespace

JointController::JointController(Twin twin) : twin_(std::move(twin)) {}

void JointController::refresh() const
{
    auto* a = api(twin_.client());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto states = a->srcAppApiUrdfGetTwinJointStates(from_std(twin_.uuid())).get();
        (void)states;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(std::string(e.what()), 0);
    }
    // JointController is stateless in C++; get/list/get_all always call refresh internally
    // so we don't cache. refresh() is a no-op here; callers use get/list/get_all.
}

double JointController::get(const std::string& joint_name) const
{
    auto* a = api(twin_.client());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto states = a->srcAppApiUrdfGetTwinJointStates(from_std(twin_.uuid())).get();
        if (!states || !states->nameIsSet() || !states->positionIsSet())
            throw CyberwaveError("Joint '" + joint_name + "' not found");
        const auto& names = states->getName();
        const auto& positions = states->getPosition();
        for (size_t i = 0; i < names.size() && i < positions.size(); ++i)
        {
            if (to_std(names[i]) == joint_name)
                return positions[i];
        }
        throw CyberwaveError("Joint '" + joint_name + "' not found");
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(std::string(e.what()), 0);
    }
}

void JointController::set(const std::string& joint_name, double position, bool degrees, double timestamp,
                          const std::string& source_type) const
{
    if (degrees)
        position = position * (3.141592653589793 / 180.0);
    const auto& client = twin_.client();
    auto mqtt = client.mqtt_client();
    if (mqtt && mqtt->is_connected())
    {
        const std::string& st = source_type.empty() ? client.source_type() : source_type;
        mqtt->update_joint_state(twin_.uuid(), joint_name, position, 0.0, 0.0, timestamp, st);
        return;
    }
    auto* a = api(client);
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto update = std::make_shared<org::openapitools::client::model::JointStateUpdateSchema>();
        update->setPosition(position);
        a->srcAppApiUrdfUpdateTwinJointState(from_std(twin_.uuid()), from_std(joint_name), update).get();
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(std::string(e.what()), 0);
    }
}

std::vector<std::string> JointController::list() const
{
    auto* a = api(twin_.client());
    if (!a)
        return {};
    try
    {
        auto states = a->srcAppApiUrdfGetTwinJointStates(from_std(twin_.uuid())).get();
        std::vector<std::string> out;
        if (!states || !states->nameIsSet())
            return out;
        for (const auto& n : states->getName())
            out.push_back(to_std(n));
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(std::string(e.what()), 0);
    }
}

std::map<std::string, double> JointController::get_all() const
{
    auto* a = api(twin_.client());
    if (!a)
        return {};
    try
    {
        auto states = a->srcAppApiUrdfGetTwinJointStates(from_std(twin_.uuid())).get();
        std::map<std::string, double> out;
        if (!states || !states->nameIsSet() || !states->positionIsSet())
            return out;
        const auto& names = states->getName();
        const auto& positions = states->getPosition();
        for (size_t i = 0; i < names.size() && i < positions.size(); ++i)
            out[to_std(names[i])] = positions[i];
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(std::string(e.what()), 0);
    }
}

} // namespace cyberwave
