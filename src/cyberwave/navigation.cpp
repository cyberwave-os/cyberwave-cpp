#include "cyberwave/navigation.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twin.h"

#include <chrono>

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/NavigationRotationSchema.h"
#include "CppRestOpenAPIClient/model/NavigationWaypointSchema.h"
#include "CppRestOpenAPIClient/model/TwinNavigationCommandSchema.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>
#include <pplx/pplxtasks.h>

#include <cmath>

namespace cyberwave
{

namespace
{

org::openapitools::client::api::DefaultApi* api(const Client& client) { return ClientAccess::default_api(&client); }

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>
json_object_to_anytype_map(const web::json::object& object)
{
    std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>> result;
    for (const auto& kv : object)
    {
        auto any_val = std::make_shared<org::openapitools::client::model::AnyType>();
        any_val->fromJson(kv.second);
        result[kv.first] = any_val;
    }
    return result;
}

std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>
string_map_to_anytype_map(const std::map<std::string, std::string>& values)
{
    std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>> result;
    for (const auto& kv : values)
    {
        auto any_val = std::make_shared<org::openapitools::client::model::AnyType>();
        any_val->fromJson(web::json::value::string(from_std(kv.second)));
        result[from_std(kv.first)] = any_val;
    }
    return result;
}

std::shared_ptr<org::openapitools::client::model::NavigationWaypointSchema> make_waypoint(double x, double y, double z,
                                                                                          double yaw)
{
    auto wp = std::make_shared<org::openapitools::client::model::NavigationWaypointSchema>();
    std::map<utility::string_t, double> pos;
    pos[from_std("x")] = x;
    pos[from_std("y")] = y;
    pos[from_std("z")] = z;
    wp->setPosition(pos);
    wp->setYaw(yaw);
    return wp;
}

// Parse a JSON object string into an AnyType map.
std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>
json_to_anytype_map(const std::string& json_str)
{
    std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>> result;
    if (json_str.empty() || json_str == "{}")
        return result;
    auto parsed = web::json::value::parse(utility::conversions::to_string_t(json_str));
    if (!parsed.is_object())
    {
        throw CyberwaveValidationError("navigation JSON payload must be an object");
    }
    return json_object_to_anytype_map(parsed.as_object());
}

// Build a NavigationRotationSchema from [w,x,y,z] or from yaw.
std::shared_ptr<org::openapitools::client::model::NavigationRotationSchema>
make_rotation_schema(const std::vector<double>& rot, double yaw)
{
    auto r = std::make_shared<org::openapitools::client::model::NavigationRotationSchema>();
    if (rot.size() >= 4)
    {
        r->setW(rot[0]);
        r->setX(rot[1]);
        r->setY(rot[2]);
        r->setZ(rot[3]);
    }
    else
    {
        double half = yaw * 0.5;
        r->setW(std::cos(half));
        r->setX(0.0);
        r->setY(0.0);
        r->setZ(std::sin(half));
    }
    return r;
}

std::vector<std::shared_ptr<org::openapitools::client::model::NavigationWaypointSchema>>
build_plan_waypoints(const NavigationPlan& plan)
{
    const auto parsed = web::json::value::parse(from_std(plan.build()));
    if (!parsed.is_object())
        throw CyberwaveValidationError("navigation plan must serialize to an object");

    const auto& obj = parsed.as_object();
    auto waypoints_it = obj.find(from_std("waypoints"));
    if (waypoints_it == obj.end() || !waypoints_it->second.is_array())
        throw CyberwaveValidationError("navigation plan must contain a waypoint array");

    std::vector<std::shared_ptr<org::openapitools::client::model::NavigationWaypointSchema>> waypoints;
    for (const auto& waypoint_value : waypoints_it->second.as_array())
    {
        if (!waypoint_value.is_object())
            throw CyberwaveValidationError("navigation waypoint must be an object");

        const auto& waypoint = waypoint_value.as_object();
        auto pos_it = waypoint.find(from_std("position"));
        if (pos_it == waypoint.end() || !pos_it->second.is_object())
            throw CyberwaveValidationError("navigation waypoint must contain an object position");

        const auto& position = pos_it->second.as_object();
        auto x_it = position.find(from_std("x"));
        auto y_it = position.find(from_std("y"));
        auto z_it = position.find(from_std("z"));
        if (x_it == position.end() || y_it == position.end() || z_it == position.end() || !x_it->second.is_number() ||
            !y_it->second.is_number() || !z_it->second.is_number())
        {
            throw CyberwaveValidationError("navigation waypoint position must contain numeric x, y, and z");
        }

        double yaw = 0.0;
        auto yaw_it = waypoint.find(from_std("yaw"));
        if (yaw_it != waypoint.end())
        {
            if (!yaw_it->second.is_number())
                throw CyberwaveValidationError("navigation waypoint yaw must be numeric");
            yaw = yaw_it->second.as_double();
        }

        auto schema = make_waypoint(x_it->second.as_double(), y_it->second.as_double(), z_it->second.as_double(), yaw);

        auto id_it = waypoint.find(from_std("id"));
        if (id_it != waypoint.end())
        {
            if (!id_it->second.is_string())
                throw CyberwaveValidationError("navigation waypoint id must be a string");
            schema->setId(id_it->second.as_string());
        }

        auto rotation_it = waypoint.find(from_std("rotation"));
        if (rotation_it != waypoint.end())
        {
            if (!rotation_it->second.is_object())
                throw CyberwaveValidationError("navigation waypoint rotation must be an object");
            const auto& rotation = rotation_it->second.as_object();
            auto w_it = rotation.find(from_std("w"));
            auto rx_it = rotation.find(from_std("x"));
            auto ry_it = rotation.find(from_std("y"));
            auto rz_it = rotation.find(from_std("z"));
            if (w_it == rotation.end() || rx_it == rotation.end() || ry_it == rotation.end() ||
                rz_it == rotation.end() || !w_it->second.is_number() || !rx_it->second.is_number() ||
                !ry_it->second.is_number() || !rz_it->second.is_number())
            {
                throw CyberwaveValidationError("navigation waypoint rotation must contain numeric w, x, y, and z");
            }
            schema->setRotation(make_rotation_schema({w_it->second.as_double(), rx_it->second.as_double(),
                                                      ry_it->second.as_double(), rz_it->second.as_double()},
                                                     yaw));
        }

        auto metadata_it = waypoint.find(from_std("metadata"));
        if (metadata_it != waypoint.end())
        {
            if (!metadata_it->second.is_object())
                throw CyberwaveValidationError("navigation waypoint metadata must be an object");
            schema->setMetadata(json_object_to_anytype_map(metadata_it->second.as_object()));
        }

        waypoints.push_back(schema);
    }

    return waypoints;
}

} // namespace

// --- NavigationPlan ---

NavigationPlan::NavigationPlan() : plan_id_("plan"), name_("plan") {}

NavigationPlan::NavigationPlan(std::string plan_id) : plan_id_(std::move(plan_id)), name_(plan_id_) {}

NavigationPlan& NavigationPlan::set_name(std::string name)
{
    name_ = std::move(name);
    return *this;
}

NavigationPlan& NavigationPlan::with_controller(std::string policy_uuid)
{
    controller_policy_uuid_ = std::move(policy_uuid);
    return *this;
}

NavigationPlan& NavigationPlan::set_metadata(std::map<std::string, std::string> metadata)
{
    metadata_ = std::move(metadata);
    return *this;
}

NavigationPlan& NavigationPlan::waypoint(double x, double y, double z, double yaw)
{
    waypoints_.push_back({x, y, z, yaw, "", {}, "{}"});
    return *this;
}

NavigationPlan& NavigationPlan::waypoint(double x, double y, double z, double yaw, const std::string& id,
                                         const std::vector<double>& rotation, const std::string& metadata_json)
{
    waypoints_.push_back({x, y, z, yaw, id, rotation, metadata_json.empty() ? "{}" : metadata_json});
    return *this;
}

NavigationPlan& NavigationPlan::extend(const std::vector<std::vector<double>>& waypoints)
{
    for (const auto& w : waypoints)
    {
        double x = w.size() > 0 ? w[0] : 0.0;
        double y = w.size() > 1 ? w[1] : 0.0;
        double z = w.size() > 2 ? w[2] : 0.0;
        double yaw = w.size() > 3 ? w[3] : 0.0;
        waypoints_.push_back({x, y, z, yaw, "", {}, "{}"});
    }
    return *this;
}

std::vector<std::vector<double>> NavigationPlan::build_waypoints() const
{
    std::vector<std::vector<double>> out;
    for (const auto& w : waypoints_)
    {
        out.push_back({w.x, w.y, w.z, w.yaw});
    }
    return out;
}

std::string NavigationPlan::build() const
{
    web::json::value obj = web::json::value::object();
    obj[utility::conversions::to_string_t("id")] =
        web::json::value::string(utility::conversions::to_string_t(plan_id_));
    obj[utility::conversions::to_string_t("name")] = web::json::value::string(utility::conversions::to_string_t(name_));
    obj[utility::conversions::to_string_t("controller_policy_uuid")] =
        web::json::value::string(utility::conversions::to_string_t(controller_policy_uuid_));

    // metadata map
    web::json::value meta = web::json::value::object();
    for (const auto& kv : metadata_)
        meta[utility::conversions::to_string_t(kv.first)] =
            web::json::value::string(utility::conversions::to_string_t(kv.second));
    obj[utility::conversions::to_string_t("metadata")] = meta;

    // waypoints array
    web::json::value wps = web::json::value::array();
    int idx = 0;
    for (const auto& w : waypoints_)
    {
        web::json::value wp = web::json::value::object();
        web::json::value pos = web::json::value::object();
        pos[utility::conversions::to_string_t("x")] = web::json::value::number(w.x);
        pos[utility::conversions::to_string_t("y")] = web::json::value::number(w.y);
        pos[utility::conversions::to_string_t("z")] = web::json::value::number(w.z);
        wp[utility::conversions::to_string_t("position")] = pos;
        wp[utility::conversions::to_string_t("yaw")] = web::json::value::number(w.yaw);
        if (!w.id.empty())
            wp[utility::conversions::to_string_t("id")] =
                web::json::value::string(utility::conversions::to_string_t(w.id));
        if (w.rotation.size() >= 4)
        {
            web::json::value rot = web::json::value::object();
            rot[utility::conversions::to_string_t("w")] = web::json::value::number(w.rotation[0]);
            rot[utility::conversions::to_string_t("x")] = web::json::value::number(w.rotation[1]);
            rot[utility::conversions::to_string_t("y")] = web::json::value::number(w.rotation[2]);
            rot[utility::conversions::to_string_t("z")] = web::json::value::number(w.rotation[3]);
            wp[utility::conversions::to_string_t("rotation")] = rot;
        }
        try
        {
            auto wp_meta = web::json::value::parse(utility::conversions::to_string_t(w.metadata_json));
            wp[utility::conversions::to_string_t("metadata")] = wp_meta;
        }
        catch (const web::json::json_exception&)
        {
            throw CyberwaveValidationError("navigation waypoint metadata_json must be valid JSON");
        }
        wps[idx++] = wp;
    }
    obj[utility::conversions::to_string_t("waypoints")] = wps;

    return utility::conversions::to_utf8string(obj.serialize());
}

NavigationPlan::MissionPayload NavigationPlan::to_mission(const std::string& twin_uuid, const std::string& mission_name,
                                                          const std::string& mission_id, const std::string& description,
                                                          const std::string& created_at,
                                                          std::optional<bool> is_active) const
{
    MissionPayload p;
    p.id = mission_id.empty() ? plan_id_ : mission_id;
    p.name = mission_name.empty() ? name_ : mission_name;
    p.description = description;
    p.twin_uuid = twin_uuid;
    p.controller_policy_uuid = controller_policy_uuid_;
    if (created_at.empty())
    {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&t));
        p.created_at = std::string(buf);
    }
    else
    {
        p.created_at = created_at;
    }
    p.is_active = is_active;
    p.waypoints = build_waypoints();
    p.metadata = metadata_;
    return p;
}

// --- TwinNavigationHandle ---

TwinNavigationHandle::TwinNavigationHandle(Twin twin) : twin_(std::move(twin)), controller_policy_uuid_() {}

const std::string& TwinNavigationHandle::twin_uuid() const { return twin_.uuid(); }

TwinNavigationHandle& TwinNavigationHandle::use_controller(const std::string& policy_uuid)
{
    controller_policy_uuid_ = policy_uuid;
    return *this;
}

TwinNavigationHandle& TwinNavigationHandle::clear_controller()
{
    controller_policy_uuid_.clear();
    return *this;
}

NavigationPlan TwinNavigationHandle::plan() const { return NavigationPlan(); }

NavigationPlan TwinNavigationHandle::plan(const std::string& plan_id) const { return NavigationPlan(plan_id); }

void TwinNavigationHandle::goto_position(double x, double y, double z, double yaw, const std::string& environment_uuid,
                                         const std::string& source_type, const std::vector<double>& rotation,
                                         const std::string& constraints_json, const std::string& metadata_json) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;
    auto cmd = std::make_shared<org::openapitools::client::model::TwinNavigationCommandSchema>();
    cmd->setCommand(org::openapitools::client::model::TwinNavigationCommandSchema::CommandEnum::GOTO);
    cmd->setPosition({x, y, z});
    cmd->setYaw(yaw);
    if (!controller_policy_uuid_.empty())
        cmd->setControllerPolicyUuid(from_std(controller_policy_uuid_));
    if (!environment_uuid.empty())
        cmd->setEnvironmentUuid(from_std(environment_uuid));
    if (!source_type.empty())
        cmd->setSourceType(from_std(source_type));
    if (rotation.size() >= 4)
    {
        cmd->setRotation(rotation);
    }
    if (!constraints_json.empty() && constraints_json != "{}")
        cmd->setConstraints(json_to_anytype_map(constraints_json));
    if (!metadata_json.empty() && metadata_json != "{}")
        cmd->setMetadata(json_to_anytype_map(metadata_json));
    a->srcAppApiNavigationExecuteTwinNavigation(from_std(twin_.uuid()), cmd).get();
}

void TwinNavigationHandle::follow_path(const std::vector<std::vector<double>>& waypoints, double wait_s, int max_loops,
                                       const std::string& environment_uuid, const std::string& source_type,
                                       const std::string& constraints_json, const std::string& metadata_json) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;
    std::vector<std::shared_ptr<org::openapitools::client::model::NavigationWaypointSchema>> list;
    for (const auto& w : waypoints)
    {
        double x = w.size() > 0 ? w[0] : 0;
        double y = w.size() > 1 ? w[1] : 0;
        double z = w.size() > 2 ? w[2] : 0;
        double yaw = w.size() > 3 ? w[3] : 0;
        list.push_back(make_waypoint(x, y, z, yaw));
    }
    auto cmd = std::make_shared<org::openapitools::client::model::TwinNavigationCommandSchema>();
    cmd->setCommand(org::openapitools::client::model::TwinNavigationCommandSchema::CommandEnum::PATH);
    cmd->setWaypoints(list);
    if (!controller_policy_uuid_.empty())
        cmd->setControllerPolicyUuid(from_std(controller_policy_uuid_));
    if (!environment_uuid.empty())
        cmd->setEnvironmentUuid(from_std(environment_uuid));
    if (!source_type.empty())
        cmd->setSourceType(from_std(source_type));
    if (!constraints_json.empty() && constraints_json != "{}")
        cmd->setConstraints(json_to_anytype_map(constraints_json));
    // wait_s and max_loops are encoded into metadata (mirrors Python SDK)
    auto nav_meta = json_to_anytype_map(metadata_json);
    if (wait_s > 0.0)
    {
        auto any_wait = std::make_shared<org::openapitools::client::model::AnyType>();
        any_wait->fromJson(web::json::value::number(wait_s));
        nav_meta[utility::conversions::to_string_t("wait_s")] = any_wait;
    }
    if (max_loops > 1)
    {
        auto any_loops = std::make_shared<org::openapitools::client::model::AnyType>();
        any_loops->fromJson(web::json::value::number(max_loops));
        nav_meta[utility::conversions::to_string_t("max_loops")] = any_loops;
    }
    if (!nav_meta.empty())
        cmd->setMetadata(nav_meta);
    a->srcAppApiNavigationExecuteTwinNavigation(from_std(twin_.uuid()), cmd).get();
}

void TwinNavigationHandle::follow_path(const NavigationPlan& plan, double wait_s, int max_loops) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;

    auto cmd = std::make_shared<org::openapitools::client::model::TwinNavigationCommandSchema>();
    cmd->setCommand(org::openapitools::client::model::TwinNavigationCommandSchema::CommandEnum::PATH);
    cmd->setWaypoints(build_plan_waypoints(plan));

    const std::string& controller =
        controller_policy_uuid_.empty() ? plan.controller_policy_uuid() : controller_policy_uuid_;
    if (!controller.empty())
        cmd->setControllerPolicyUuid(from_std(controller));

    auto nav_meta = string_map_to_anytype_map(plan.metadata());
    if (wait_s > 0.0)
    {
        auto any_wait = std::make_shared<org::openapitools::client::model::AnyType>();
        any_wait->fromJson(web::json::value::number(wait_s));
        nav_meta[from_std("wait_s")] = any_wait;
    }
    if (max_loops > 1)
    {
        auto any_loops = std::make_shared<org::openapitools::client::model::AnyType>();
        any_loops->fromJson(web::json::value::number(max_loops));
        nav_meta[from_std("max_loops")] = any_loops;
    }
    if (!nav_meta.empty())
        cmd->setMetadata(nav_meta);

    a->srcAppApiNavigationExecuteTwinNavigation(from_std(twin_.uuid()), cmd).get();
}

void TwinNavigationHandle::stop(const std::string& environment_uuid, const std::string& source_type) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;
    auto cmd = std::make_shared<org::openapitools::client::model::TwinNavigationCommandSchema>();
    cmd->setCommand(org::openapitools::client::model::TwinNavigationCommandSchema::CommandEnum::STOP);
    if (!controller_policy_uuid_.empty())
        cmd->setControllerPolicyUuid(from_std(controller_policy_uuid_));
    if (!environment_uuid.empty())
        cmd->setEnvironmentUuid(from_std(environment_uuid));
    if (!source_type.empty())
        cmd->setSourceType(from_std(source_type));
    a->srcAppApiNavigationExecuteTwinNavigation(from_std(twin_.uuid()), cmd).get();
}

void TwinNavigationHandle::pause(const std::string& environment_uuid, const std::string& source_type) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;
    auto cmd = std::make_shared<org::openapitools::client::model::TwinNavigationCommandSchema>();
    cmd->setCommand(org::openapitools::client::model::TwinNavigationCommandSchema::CommandEnum::PAUSE);
    if (!controller_policy_uuid_.empty())
        cmd->setControllerPolicyUuid(from_std(controller_policy_uuid_));
    if (!environment_uuid.empty())
        cmd->setEnvironmentUuid(from_std(environment_uuid));
    if (!source_type.empty())
        cmd->setSourceType(from_std(source_type));
    a->srcAppApiNavigationExecuteTwinNavigation(from_std(twin_.uuid()), cmd).get();
}

void TwinNavigationHandle::resume(const std::string& environment_uuid, const std::string& source_type) const
{
    auto* a = api(twin_.client());
    if (!a)
        return;
    auto cmd = std::make_shared<org::openapitools::client::model::TwinNavigationCommandSchema>();
    cmd->setCommand(org::openapitools::client::model::TwinNavigationCommandSchema::CommandEnum::RESUME);
    if (!controller_policy_uuid_.empty())
        cmd->setControllerPolicyUuid(from_std(controller_policy_uuid_));
    if (!environment_uuid.empty())
        cmd->setEnvironmentUuid(from_std(environment_uuid));
    if (!source_type.empty())
        cmd->setSourceType(from_std(source_type));
    a->srcAppApiNavigationExecuteTwinNavigation(from_std(twin_.uuid()), cmd).get();
}

} // namespace cyberwave
