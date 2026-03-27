#include "cyberwave/twin.h"
#include "cyberwave/alerts.h"
#include "cyberwave/client.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/joints.h"
#include "cyberwave/keyboard.h"
#include "cyberwave/motion.h"
#include "cyberwave/mqtt_interface.h"
#include "cyberwave/navigation.h"
#include "cyberwave/twins.h"

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/model/TwinSchema.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>

#include <cmath>

namespace cyberwave
{

namespace
{

std::string twin_to_std(const utility::string_t& t)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return std::string(t);
#else
    return utility::conversions::to_utf8string(t);
#endif
}

static std::shared_ptr<org::openapitools::client::model::TwinSchema> twin_schema(const std::shared_ptr<void>& p)
{
    return std::static_pointer_cast<org::openapitools::client::model::TwinSchema>(p);
}

} // namespace

Twin::Twin(const Client& client, std::string uuid, std::string name)
    : client_(client), uuid_(std::move(uuid)), name_(std::move(name))
{
}

Twin::Twin(const Client& client, std::shared_ptr<void> schema_ptr) : client_(client), schema_(std::move(schema_ptr))
{
    auto s = twin_schema(schema_);
    if (s)
    {
        if (s->uuidIsSet())
            uuid_ = twin_to_std(s->getUuid());
        if (s->nameIsSet())
            name_ = twin_to_std(s->getName());
        if (s->environmentUuidIsSet())
            environment_id_ = twin_to_std(s->getEnvironmentUuid());
    }
}

std::string Twin::asset_id() const
{
    auto s = twin_schema(schema_);
    return s && s->assetUuidIsSet() ? twin_to_std(s->getAssetUuid()) : "";
}

std::string Twin::capabilities_json() const
{
    auto s = twin_schema(schema_);
    if (!s || !s->capabilitiesIsSet())
        return "{}";
    web::json::value obj = web::json::value::object();
    for (const auto& kv : s->getCapabilities())
    {
        if (kv.second)
            obj[kv.first] = kv.second->toJson();
    }
    return twin_to_std(obj.serialize());
}

std::vector<std::string> Twin::child_uuids() const
{
    auto s = twin_schema(schema_);
    std::vector<std::string> out;
    if (!s || !s->childTwinUuidsIsSet())
        return out;
    for (const auto& u : s->getChildTwinUuids())
        out.push_back(twin_to_std(u));
    return out;
}

bool Twin::has_capability(const std::string& cap) const
{
    auto s = twin_schema(schema_);
    if (!s || !s->capabilitiesIsSet())
        return false;
    auto caps = s->getCapabilities();
    return caps.find(utility::conversions::to_string_t(cap)) != caps.end();
}

bool Twin::has_sensor(const std::string& sensor_type) const
{
    if (sensor_type.empty())
    {
        // Any camera/sensor capability counts
        return has_capability("has_sensors") || has_capability("has_depth");
    }
    return has_capability(sensor_type);
}

void Twin::refresh()
{
    Twin updated = client().twins().get(uuid_);
    name_ = updated.name_;
    environment_id_ = updated.environment_id_;
    schema_ = updated.schema_;
}

void Twin::delete_twin() { client().twins().delete_twin(uuid_); }

TwinAlertManager Twin::alerts() const { return TwinAlertManager(std::make_shared<Twin>(*this)); }

TwinNavigationHandle Twin::navigation() const { return TwinNavigationHandle(*this); }

TwinMotionHandle Twin::motion() const { return TwinMotionHandle(*this); }

JointController Twin::joints() const { return JointController(*this); }

TwinControllerHandle Twin::controller() const { return TwinControllerHandle(*this); }

void Twin::edit_position(double x, double y, double z) { client().twins().update_position(uuid_, x, y, z); }

void Twin::edit_rotation(double w, double rx, double ry, double rz)
{
    client().twins().update_rotation(uuid_, w, rx, ry, rz);
}

void Twin::edit_rotation(double yaw, double pitch, double roll)
{
    // Convert Euler angles (degrees) to quaternion
    double yr = yaw * M_PI / 180.0;
    double pr = pitch * M_PI / 180.0;
    double rr = roll * M_PI / 180.0;
    double cy = std::cos(yr * 0.5), sy = std::sin(yr * 0.5);
    double cp = std::cos(pr * 0.5), sp = std::sin(pr * 0.5);
    double cr = std::cos(rr * 0.5), sr = std::sin(rr * 0.5);
    double qw = cr * cp * cy + sr * sp * sy;
    double qx = sr * cp * cy - cr * sp * sy;
    double qy = cr * sp * cy + sr * cp * sy;
    double qz = cr * cp * sy - sr * sp * cy;
    edit_rotation(qw, qx, qy, qz);
}

std::optional<Twin> Twin::parent() const
{
    auto s = twin_schema(schema_);
    if (!s || !s->attachToTwinUuidIsSet())
        return std::nullopt;
    const auto parent_uuid = twin_to_std(s->getAttachToTwinUuid());
    if (parent_uuid.empty())
        return std::nullopt;
    return client().twins().get(parent_uuid);
}

std::vector<Twin> Twin::children() const
{
    std::vector<Twin> result;
    for (const auto& uuid : child_uuids())
        result.push_back(client().twins().get(uuid));
    return result;
}

void Twin::edit_scale(double /*x*/, double /*y*/, double /*z*/)
{
    throw CyberwaveError("edit_scale() is not yet supported by the REST API");
}

std::map<std::string, double> Twin::get_joint_states() const { return client().twins().get_joint_states(uuid_); }

void Twin::update_joint_state(const std::string& joint_name, double position, double velocity, double effort) const
{
    client().twins().update_joint_state(uuid_, joint_name, position, velocity, effort);
}

std::string Twin::get_calibration(const std::string& robot_type) const
{
    return client().twins().get_calibration(uuid_, robot_type);
}

std::string Twin::update_calibration(const std::string& calibration_json, const std::string& robot_type) const
{
    return client().twins().update_calibration(uuid_, calibration_json, robot_type);
}

std::vector<unsigned char> Twin::get_latest_frame(bool mock, const std::string& sensor_id) const
{
    return client().twins().get_latest_frame(uuid_, mock, sensor_id);
}

std::string Twin::get_schema(const std::string& path) const
{
    return client().twins().get_universal_schema_at_path(uuid_, path);
}

std::string Twin::update_schema(const std::string& path, const std::string& value_json, const std::string& op) const
{
    return client().twins().patch_universal_schema(uuid_, path, value_json, op);
}

std::vector<std::string> Twin::get_controllable_joint_names() const
{
    // Parse the universal schema JSON for joint definitions with movable types.
    std::string schema_json = get_schema();
    std::vector<std::string> result;
    try
    {
        web::json::value parsed = web::json::value::parse(utility::conversions::to_string_t(schema_json));
        const auto type_key = utility::conversions::to_string_t("type");
        const auto revolute = utility::conversions::to_string_t("revolute");
        const auto prismatic = utility::conversions::to_string_t("prismatic");
        const auto continuous = utility::conversions::to_string_t("continuous");
        // Look for joint definitions with movable types in the schema
        auto find_joints = [&](const web::json::value& v, auto& self) -> void
        {
            if (!v.is_object())
                return;
            for (const auto& kv : v.as_object())
            {
                const auto& key = kv.first;
                const auto& val = kv.second;
                if (val.is_object())
                {
                    bool is_movable = false;
                    if (val.has_field(type_key))
                    {
                        auto t = val.at(type_key).as_string();
                        is_movable = (t == revolute || t == prismatic || t == continuous);
                    }
                    if (is_movable)
                        result.push_back(twin_to_std(key));
                    self(val, self);
                }
            }
        };
        find_joints(parsed, find_joints);
    }
    catch (...)
    { /* return whatever was found */
    }
    return result;
}

void Twin::subscribe(MqttMessageHandler on_update) const
{
    auto mqtt = client().mqtt_client();
    if (!mqtt)
        throw CyberwaveError("subscribe() requires an MQTT client set on Client (set_mqtt_client)");
    mqtt->subscribe_twin(uuid_, std::move(on_update));
}

void Twin::subscribe_position(MqttMessageHandler on_update) const
{
    auto mqtt = client().mqtt_client();
    if (!mqtt)
        throw CyberwaveError("subscribe_position() requires an MQTT client set on Client (set_mqtt_client)");
    mqtt->subscribe_twin_position(uuid_, std::move(on_update));
}

void Twin::subscribe_rotation(MqttMessageHandler on_update) const
{
    auto mqtt = client().mqtt_client();
    if (!mqtt)
        throw CyberwaveError("subscribe_rotation() requires an MQTT client set on Client (set_mqtt_client)");
    mqtt->subscribe_twin_rotation(uuid_, std::move(on_update));
}

void Twin::subscribe_joints(MqttMessageHandler on_update) const
{
    auto mqtt = client().mqtt_client();
    if (!mqtt)
        throw CyberwaveError("subscribe_joints() requires an MQTT client set on Client (set_mqtt_client)");
    mqtt->subscribe_twin_joint_states(uuid_, std::move(on_update));
}

} // namespace cyberwave
