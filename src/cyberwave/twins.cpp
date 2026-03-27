#include "cyberwave/twins.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twin.h"

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/JointCalibrationSchema.h"
#include "CppRestOpenAPIClient/model/JointStateUpdateSchema.h"
#include "CppRestOpenAPIClient/model/JointStatesSchema.h"
#include "CppRestOpenAPIClient/model/TwinCreateSchema.h"
#include "CppRestOpenAPIClient/model/TwinJointCalibrationSchema.h"
#include "CppRestOpenAPIClient/model/TwinSchema.h"
#include "CppRestOpenAPIClient/model/TwinStateUpdateSchema.h"
#include "CppRestOpenAPIClient/model/TwinUniversalSchemaPatchSchema.h"

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

static Twin twin_from_schema(const Client& client,
                             const std::shared_ptr<org::openapitools::client::model::TwinSchema>& ts)
{
    if (!ts)
        return Twin(client, "", "");
    return Twin(client, std::static_pointer_cast<void>(ts));
}

} // namespace

TwinManager::TwinManager(const Client& client) : client_(client) {}

std::vector<Twin> TwinManager::list(const std::string& environment_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        std::vector<std::shared_ptr<org::openapitools::client::model::TwinSchema>> vec;
        if (!environment_id.empty())
        {
            vec = a->srcAppApiEnvironmentsGetEnvironmentTwins(from_std(environment_id)).get();
        }
        else
        {
            vec = a->srcAppApiTwinsListAllTwins().get();
        }
        std::vector<Twin> out;
        for (auto& ptr : vec)
        {
            if (ptr)
                out.push_back(twin_from_schema(client_.get(), ptr));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Twin TwinManager::get(const std::string& twin_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiTwinsGetTwin(from_std(twin_id)).get();
        return twin_from_schema(client_.get(), result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Twin TwinManager::create(const std::string& asset_id, const std::string& environment_id, const std::string& name,
                         const std::string& description, const std::vector<double>& position,
                         const std::vector<double>& orientation, bool fixed_base) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::TwinCreateSchema>();
        body->setAssetUuid(from_std(asset_id));
        body->setEnvironmentUuid(from_std(environment_id));
        if (!name.empty())
            body->setName(from_std(name));
        if (!description.empty())
            body->setDescription(from_std(description));
        if (position.size() >= 3)
        {
            body->setPositionX(position[0]);
            body->setPositionY(position[1]);
            body->setPositionZ(position[2]);
        }
        if (orientation.size() >= 4)
        {
            body->setRotationW(orientation[0]);
            body->setRotationX(orientation[1]);
            body->setRotationY(orientation[2]);
            body->setRotationZ(orientation[3]);
        }
        if (fixed_base)
            body->setFixedBase(true);
        auto result = a->srcAppApiTwinsCreateTwin(body).get();
        return twin_from_schema(client_.get(), result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Twin TwinManager::update(const std::string& twin_id, const std::string& name, const std::string& description) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::TwinCreateSchema>();
        if (!name.empty())
            body->setName(from_std(name));
        if (!description.empty())
            body->setDescription(from_std(description));
        auto result = a->srcAppApiTwinsUpdateTwin(from_std(twin_id), body).get();
        return twin_from_schema(client_.get(), result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Twin TwinManager::update(const std::string& twin_id, const std::string& name, const std::string& description,
                         const std::string& attach_to_twin_uuid, const std::string& attach_to_link,
                         const std::vector<double>& offset_position, const std::vector<double>& offset_rotation) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::TwinCreateSchema>();
        if (!name.empty())
            body->setName(from_std(name));
        if (!description.empty())
            body->setDescription(from_std(description));
        if (!attach_to_twin_uuid.empty())
        {
            body->setAttachToTwinUuid(from_std(attach_to_twin_uuid));
            if (!attach_to_link.empty())
                body->setAttachToLink(from_std(attach_to_link));
            if (offset_position.size() >= 3)
            {
                body->setAttachOffsetX(offset_position[0]);
                body->setAttachOffsetY(offset_position[1]);
                body->setAttachOffsetZ(offset_position[2]);
            }
            if (offset_rotation.size() >= 4)
            {
                body->setAttachOffsetRotationW(offset_rotation[0]);
                body->setAttachOffsetRotationX(offset_rotation[1]);
                body->setAttachOffsetRotationY(offset_rotation[2]);
                body->setAttachOffsetRotationZ(offset_rotation[3]);
            }
        }
        else
        {
            body->unsetAttach_to_twin_uuid();
            body->unsetAttach_to_link();
        }
        auto result = a->srcAppApiTwinsUpdateTwin(from_std(twin_id), body).get();
        return twin_from_schema(client_.get(), result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Twin TwinManager::update_state(const std::string& twin_id, double position_x, double position_y, double position_z,
                               double rotation_w, double rotation_x, double rotation_y, double rotation_z) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::TwinStateUpdateSchema>();
        body->setPositionX(position_x);
        body->setPositionY(position_y);
        body->setPositionZ(position_z);
        body->setRotationW(rotation_w);
        body->setRotationX(rotation_x);
        body->setRotationY(rotation_y);
        body->setRotationZ(rotation_z);
        auto result = a->srcAppApiTwinsUpdateTwinState(from_std(twin_id), body).get();
        return twin_from_schema(client_.get(), result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Twin TwinManager::update_position(const std::string& twin_id, double x, double y, double z) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::TwinStateUpdateSchema>();
        body->setPositionX(x);
        body->setPositionY(y);
        body->setPositionZ(z);
        auto result = a->srcAppApiTwinsUpdateTwinState(from_std(twin_id), body).get();
        return twin_from_schema(client_.get(), result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Twin TwinManager::update_rotation(const std::string& twin_id, double w, double rx, double ry, double rz) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::TwinStateUpdateSchema>();
        body->setRotationW(w);
        body->setRotationX(rx);
        body->setRotationY(ry);
        body->setRotationZ(rz);
        auto result = a->srcAppApiTwinsUpdateTwinState(from_std(twin_id), body).get();
        return twin_from_schema(client_.get(), result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

void TwinManager::delete_twin(const std::string& twin_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        a->srcAppApiTwinsDeleteTwin(from_std(twin_id)).get();
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::map<std::string, double> TwinManager::get_joint_states(const std::string& twin_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiUrdfGetTwinJointStates(from_std(twin_id)).get();
        std::map<std::string, double> out;
        if (!result || !result->nameIsSet() || !result->positionIsSet())
            return out;
        auto names = result->getName();
        auto positions = result->getPosition();
        for (size_t i = 0; i < names.size() && i < positions.size(); ++i)
            out[to_std(names[i])] = positions[i];
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

void TwinManager::update_joint_state(const std::string& twin_id, const std::string& joint_name, double position,
                                     double velocity, double effort) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::JointStateUpdateSchema>();
        body->setPosition(position);
        if (velocity != 0.0)
            body->setVelocity(velocity);
        if (effort != 0.0)
            body->setEffort(effort);
        a->srcAppApiUrdfUpdateTwinJointState(from_std(twin_id), from_std(joint_name), body).get();
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::string TwinManager::get_calibration(const std::string& twin_id, const std::string& /*robot_type*/) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiTwinsGetTwinCalibration(from_std(twin_id)).get();
        if (!result)
            return "{}";
        return to_std(result->toJson().serialize());
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::string TwinManager::update_calibration(const std::string& twin_id, const std::string& calibration_json,
                                            const std::string& robot_type) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::TwinJointCalibrationSchema>();
        web::json::value parsed = web::json::value::parse(from_std(calibration_json));
        // Deserialize calibration JSON into the body's joint_calibration map
        if (parsed.is_object())
        {
            std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::JointCalibrationSchema>>
                cal_map;
            for (const auto& kv : parsed.as_object())
            {
                auto cal = std::make_shared<org::openapitools::client::model::JointCalibrationSchema>();
                cal->fromJson(kv.second);
                cal_map[kv.first] = cal;
            }
            body->setJointCalibration(cal_map);
        }
        if (!robot_type.empty())
            body->setRobotType(from_std(robot_type));
        auto result = a->srcAppApiTwinsUpdateTwinCalibration(from_std(twin_id), body).get();
        if (!result)
            return "{}";
        return to_std(result->toJson().serialize());
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::vector<unsigned char> TwinManager::get_latest_frame(const std::string& twin_id, bool mock,
                                                         const std::string& /*sensor_id*/) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        boost::optional<bool> mock_opt;
        if (mock)
            mock_opt = true;
        a->srcAppApiTwinsGetTwinLatestFrame(from_std(twin_id), mock_opt).get();
        throw CyberwaveError("get_latest_frame() is not supported by the generated C++ REST client because the binary "
                             "response body cannot be captured");
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

static std::string
any_map_to_json(const std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>& m)
{
    web::json::value obj = web::json::value::object();
    for (const auto& kv : m)
    {
        if (kv.second)
            obj[kv.first] = kv.second->toJson();
    }
    auto s = obj.serialize();
#if defined(_TURN_OFF_PLATFORM_STRING)
    return s;
#else
    return utility::conversions::to_utf8string(s);
#endif
}

std::string TwinManager::get_universal_schema_at_path(const std::string& twin_id, const std::string& path) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        boost::optional<utility::string_t> path_opt;
        if (!path.empty())
            path_opt = from_std(path);
        auto result = a->srcAppApiTwinsGetTwinUniversalSchemaAtPath(from_std(twin_id), path_opt).get();
        return any_map_to_json(result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::string TwinManager::patch_universal_schema(const std::string& twin_id, const std::string& path,
                                                const std::string& value_json, const std::string& op) const
{
    auto* a = api(client_);
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::TwinUniversalSchemaPatchSchema>();
        body->setOp(from_std(op));
        body->setPath(from_std(path));
        web::json::value val = web::json::value::parse(from_std(value_json));
        auto any_val = std::make_shared<org::openapitools::client::model::AnyType>();
        any_val->fromJson(val);
        body->setValue(any_val);
        auto result = a->srcAppApiTwinsPatchTwinUniversalSchema(from_std(twin_id), body).get();
        return any_map_to_json(result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
