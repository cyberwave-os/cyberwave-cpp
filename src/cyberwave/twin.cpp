#include "cyberwave/twin.h"
#include "cyberwave/alerts.h"
#include "cyberwave/camera_streaming.h"
#include "cyberwave/client.h"
#include "cyberwave/constants.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/joints.h"
#include "cyberwave/keyboard.h"
#include "cyberwave/locomotion_contracts.h"
#include "cyberwave/motion.h"
#include "cyberwave/mqtt_interface.h"
#include "cyberwave/navigation.h"
#include "cyberwave/twins.h"
#include "rest_helpers.h"
#include "source_type_utils.h"

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/model/TwinSchema.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <sstream>

namespace cyberwave
{

namespace
{

struct CapabilityFlags
{
    bool can_fly = false;
    bool can_locomote = false;
    bool can_grip = false;
    bool has_sensors = false;
    bool has_depth = false;
};

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

bool json_bool_or_false(const web::json::value& value)
{
    if (value.is_boolean())
        return value.as_bool();
    if (value.is_number())
        return value.as_integer() != 0;
    if (!value.is_string())
        return false;

    std::string normalized = twin_to_std(value.as_string());
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

std::string dispatch_mode_for_client(const Client& client, const std::string& mode)
{
    std::string normalized = mode.empty() ? client.config().runtime_mode : mode;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "simulation" || normalized == "sim" || normalized == "sim_tele")
        return "simulation";
    if (normalized == "preview" || normalized == "playground" || normalized == "kinematic")
        return "preview";
    if (normalized == "live" || normalized == "real-world" || normalized == "real" || normalized == "tele" ||
        normalized == "teleoperation")
        return "live";
    throw CyberwaveError("Unknown control mode '" + mode + "'. Use 'live', 'simulation', or 'preview'.");
}

web::json::value policy_ref_json(const PolicyRefPayload& policy_ref)
{
    if (policy_ref.empty())
        throw CyberwaveError("PolicyRef requires both kind and value");
    web::json::value value = web::json::value::object();
    value[detail::to_utility("kind")] = web::json::value::string(detail::to_utility(policy_ref.kind));
    value[detail::to_utility("value")] = web::json::value::string(detail::to_utility(policy_ref.value));
    return value;
}

std::string dispatch_velocity_impl(const Twin& twin, const LocomotionVelocityCommand& command, const std::string& mode,
                                   const std::string& simulation_backend, const std::string& controller_policy_uuid,
                                   const PolicyRefPayload* policy_ref)
{
    if (!twin.can_locomote())
        throw CyberwaveError("dispatch_velocity() requires a twin with locomotion capabilities");

    const std::string environment_uuid =
        !twin.environment_id().empty() ? twin.environment_id() : twin.client().config().environment_id;
    if (environment_uuid.empty())
        throw CyberwaveError("Backend velocity dispatch requires the twin environment_uuid or client environment_id");

    validate_locomotion_velocity_command(command, true);

    const std::string dispatch_mode = dispatch_mode_for_client(twin.client(), mode);
    const std::string runtime_kind = dispatch_mode == "live" ? "physical" : "simulation";
    const bool include_simulation_backend = runtime_kind == "simulation" && !simulation_backend.empty();

    web::json::value payload = web::json::value::object();
    payload[detail::to_utility("velocity_command")] = web::json::value::parse(detail::to_utility(command.to_json()));
    payload[detail::to_utility("runtime_kind")] = web::json::value::string(detail::to_utility(runtime_kind));
    if (include_simulation_backend)
    {
        payload[detail::to_utility("simulation_backend")] =
            web::json::value::string(detail::to_utility(simulation_backend));
    }
    if (policy_ref)
    {
        payload[detail::to_utility("policy_ref")] = policy_ref_json(*policy_ref);
    }
    if (!controller_policy_uuid.empty())
    {
        payload[detail::to_utility("controller_policy_uuid")] =
            web::json::value::string(detail::to_utility(controller_policy_uuid));
    }

    web::json::value action = web::json::value::object();
    action[detail::to_utility("kind")] = web::json::value::string(detail::to_utility("controller_policy_execute"));
    action[detail::to_utility("target_twin_uuid")] = web::json::value::string(detail::to_utility(twin.uuid()));
    action[detail::to_utility("payload")] = payload;
    if (policy_ref)
    {
        action[detail::to_utility("policy_ref")] = policy_ref_json(*policy_ref);
    }
    if (!controller_policy_uuid.empty())
    {
        action[detail::to_utility("controller_policy_uuid")] =
            web::json::value::string(detail::to_utility(controller_policy_uuid));
    }

    web::json::value request_body = web::json::value::object();
    request_body[detail::to_utility("action")] = action;
    request_body[detail::to_utility("mode")] = web::json::value::string(detail::to_utility(dispatch_mode));
    request_body[detail::to_utility("confirmed")] = web::json::value::boolean(dispatch_mode == "live");
    if (include_simulation_backend)
    {
        request_body[detail::to_utility("simulation_backend")] =
            web::json::value::string(detail::to_utility(simulation_backend));
    }

    const auto response = detail::request_raw(
        twin.client(),
        detail::to_utility("/api/v1/agents/environments/" + environment_uuid + "/control/actions/dispatch"),
        web::http::methods::POST, {}, request_body);
    return response.text();
}

CapabilityFlags
capability_flags_from_schema(const std::shared_ptr<org::openapitools::client::model::TwinSchema>& schema)
{
    CapabilityFlags flags;
    if (!schema || !schema->capabilitiesIsSet())
        return flags;

    const auto& caps = schema->getCapabilities();
    const auto get_value = [&](const char* key) -> web::json::value
    {
        auto it = caps.find(utility::conversions::to_string_t(key));
        if (it == caps.end() || !it->second)
            return web::json::value::null();
        return it->second->toJson();
    };

    flags.can_fly = json_bool_or_false(get_value("can_fly"));
    flags.can_locomote = json_bool_or_false(get_value("can_locomote"));
    flags.can_grip = json_bool_or_false(get_value("can_grip"));
    flags.has_sensors = json_bool_or_false(get_value("has_sensors"));
    flags.has_depth = json_bool_or_false(get_value("has_depth"));

    const auto sensors = get_value("sensors");
    if (sensors.is_array())
    {
        const auto& values = sensors.as_array();
        flags.has_sensors = flags.has_sensors || values.size() > 0;
        for (const auto& sensor : values)
        {
            if (!sensor.is_object())
                continue;
            const auto type_key = utility::conversions::to_string_t("type");
            if (sensor.has_field(type_key))
            {
                const std::string sensor_type = twin_to_std(sensor.at(type_key).as_string());
                if (sensor_type == "depth")
                    flags.has_depth = true;
            }
        }
    }

    return flags;
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

bool Twin::fixed_base() const
{
    auto s = twin_schema(schema_);
    return s && s->fixedBaseIsSet() && s->isFixedBase();
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
    const CapabilityFlags flags = capability_flags_from_schema(twin_schema(schema_));
    if (sensor_type.empty())
        return flags.has_sensors || flags.has_depth;

    std::string normalized = sensor_type;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "depth")
        return flags.has_depth;
    return flags.has_sensors;
}

bool Twin::can_fly() const { return capability_flags_from_schema(twin_schema(schema_)).can_fly; }

bool Twin::can_locomote() const { return capability_flags_from_schema(twin_schema(schema_)).can_locomote; }

bool Twin::can_grip() const { return capability_flags_from_schema(twin_schema(schema_)).can_grip; }

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

void Twin::update_joint_state(const std::string& joint_name, double position, std::optional<double> velocity,
                              std::optional<double> effort) const
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

std::vector<unsigned char> Twin::get_latest_frame(bool mock, const std::string& sensor_id,
                                                  const std::string& source_type) const
{
    return client().twins().get_latest_frame(uuid_, mock, sensor_id,
                                             detail::resolve_frame_source_type(client(), source_type));
}

void Twin::set_frame_source(std::shared_ptr<IFrameSource> source) { frame_source_ = std::move(source); }

void Twin::start_streaming(int fps, int /*camera_id*/)
{
    if (!has_sensor())
        throw CyberwaveError("start_streaming() requires a twin with camera sensor capabilities");
    if (!frame_source_)
        throw CyberwaveError("Set a frame source with set_frame_source() before start_streaming()");

    auto mqtt = client().mqtt_client();
    if (!mqtt)
        throw CyberwaveError("start_streaming() requires an MQTT client set on Client (set_mqtt_client)");

    stop_streaming();
    camera_streamer_ = std::shared_ptr<CameraStreamer>(new CameraStreamer(mqtt, uuid(), frame_source_, fps),
                                                       [](CameraStreamer* streamer)
                                                       {
                                                           if (streamer)
                                                           {
                                                               streamer->stop();
                                                               delete streamer;
                                                           }
                                                       });
    camera_streamer_->start();
}

void Twin::stop_streaming() { camera_streamer_.reset(); }

void Twin::set_depth_source(std::shared_ptr<IDepthSource> source) { depth_source_ = std::move(source); }

void Twin::start_depth_streaming(int fps)
{
    if (!has_sensor("depth"))
        throw CyberwaveError("start_depth_streaming() requires a twin with depth sensor capabilities");
    if (!depth_source_)
        throw CyberwaveError("Set a depth source with set_depth_source() before start_depth_streaming()");

    auto mqtt = client().mqtt_client();
    if (!mqtt)
        throw CyberwaveError("start_depth_streaming() requires an MQTT client set on Client (set_mqtt_client)");

    stop_depth_streaming();
    depth_streamer_ = std::shared_ptr<DepthStreamer>(new DepthStreamer(mqtt, uuid(), depth_source_, fps),
                                                     [](DepthStreamer* streamer)
                                                     {
                                                         if (streamer)
                                                         {
                                                             streamer->stop();
                                                             delete streamer;
                                                         }
                                                     });
    depth_streamer_->start();
}

void Twin::stop_depth_streaming() { depth_streamer_.reset(); }

void Twin::publish_depth_frame(const std::string& json_payload)
{
    if (!has_sensor("depth"))
        throw CyberwaveError("publish_depth_frame() requires a twin with depth sensor capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/depth", json_payload);
}

void Twin::publish_point_cloud(const std::string& json_payload)
{
    if (!has_sensor("depth"))
        throw CyberwaveError("publish_point_cloud() requires a twin with depth sensor capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/pointcloud", json_payload);
}

void Twin::capture_depth_frame() const
{
    throw CyberwaveError("capture_depth_frame() requires an active depth stream. Use start_depth_streaming() first.");
}

void Twin::get_point_cloud() const
{
    throw CyberwaveError(
        "get_point_cloud() requires depth sensor data processing. This feature is not yet implemented.");
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

void Twin::move_forward(double distance_m, const std::string& source_type)
{
    if (!can_locomote())
        throw CyberwaveError("move_forward() requires a twin with locomotion capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;

    const std::string resolved_source_type = detail::normalize_control_source_type(client(), source_type);
    const double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << resolved_source_type
        << "\",\"command\":\"move_forward\",\"data\":{\"linear_x\":" << distance_m
        << ",\"angular_z\":0},\"timestamp\":" << ts << "}";
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/command", out.str());
}

void Twin::move_backward(double distance_m, const std::string& source_type)
{
    if (!can_locomote())
        throw CyberwaveError("move_backward() requires a twin with locomotion capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    const std::string resolved_source_type = detail::normalize_control_source_type(client(), source_type);
    const double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << resolved_source_type
        << "\",\"command\":\"move_backward\",\"data\":{\"linear_x\":" << -distance_m
        << ",\"angular_z\":0},\"timestamp\":" << ts << "}";
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/command", out.str());
}

void Twin::turn_left(double angle_rad, const std::string& source_type)
{
    if (!can_locomote())
        throw CyberwaveError("turn_left() requires a twin with locomotion capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;

    const std::string resolved_source_type = detail::normalize_control_source_type(client(), source_type);
    const double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << resolved_source_type
        << "\",\"command\":\"turn_left\",\"data\":{\"linear_x\":0,\"angular_z\":" << angle_rad
        << "},\"timestamp\":" << ts << "}";
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/command", out.str());
}

void Twin::turn_right(double angle_rad, const std::string& source_type)
{
    if (!can_locomote())
        throw CyberwaveError("turn_right() requires a twin with locomotion capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;

    const std::string resolved_source_type = detail::normalize_control_source_type(client(), source_type);
    const double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << resolved_source_type
        << "\",\"command\":\"turn_right\",\"data\":{\"linear_x\":0,\"angular_z\":" << -angle_rad
        << "},\"timestamp\":" << ts << "}";
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/command", out.str());
}

std::string Twin::dispatch_velocity(const LocomotionVelocityCommand& command, const std::string& mode,
                                    const std::string& simulation_backend,
                                    const std::string& controller_policy_uuid) const
{
    return dispatch_velocity_impl(*this, command, mode, simulation_backend, controller_policy_uuid, nullptr);
}

std::string Twin::dispatch_velocity(const LocomotionVelocityCommand& command, const PolicyRefPayload& policy_ref,
                                    const std::string& mode, const std::string& simulation_backend) const
{
    return dispatch_velocity_impl(*this, command, mode, simulation_backend, "", &policy_ref);
}

std::string Twin::set_velocity(double linear_x, double linear_y, double angular_z, int duration_ms,
                               const std::string& gait, const std::string& origin, const std::string& mode,
                               const std::string& simulation_backend, const std::string& controller_policy_uuid) const
{
    return dispatch_velocity(
        build_locomotion_velocity_command(linear_x, linear_y, angular_z, duration_ms, gait, origin), mode,
        simulation_backend, controller_policy_uuid);
}

std::string Twin::drive_forward(double speed, int duration_ms, const std::string& mode,
                                const std::string& simulation_backend, const std::string& controller_policy_uuid) const
{
    return set_velocity(speed, 0.0, 0.0, duration_ms, "walk", "teleop", mode, simulation_backend,
                        controller_policy_uuid);
}

std::string Twin::drive_backward(double speed, int duration_ms, const std::string& mode,
                                 const std::string& simulation_backend, const std::string& controller_policy_uuid) const
{
    return set_velocity(-speed, 0.0, 0.0, duration_ms, "walk", "teleop", mode, simulation_backend,
                        controller_policy_uuid);
}

std::string Twin::turn_velocity_left(double angular, int duration_ms, const std::string& mode,
                                     const std::string& simulation_backend,
                                     const std::string& controller_policy_uuid) const
{
    return set_velocity(0.0, 0.0, angular, duration_ms, "walk", "teleop", mode, simulation_backend,
                        controller_policy_uuid);
}

std::string Twin::turn_velocity_right(double angular, int duration_ms, const std::string& mode,
                                      const std::string& simulation_backend,
                                      const std::string& controller_policy_uuid) const
{
    return set_velocity(0.0, 0.0, -angular, duration_ms, "walk", "teleop", mode, simulation_backend,
                        controller_policy_uuid);
}

std::string Twin::stop_velocity(const std::string& mode, const std::string& simulation_backend,
                                const std::string& controller_policy_uuid) const
{
    return dispatch_velocity(stop_locomotion_velocity_command("teleop"), mode, simulation_backend,
                             controller_policy_uuid);
}

void Twin::takeoff(double altitude_m)
{
    if (!can_fly())
        throw CyberwaveError("takeoff() requires a twin with flight capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;

    const double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << SOURCE_TYPE_TELE
        << "\",\"command\":\"takeoff\",\"data\":{\"altitude\":" << altitude_m << "},\"timestamp\":" << ts << "}";
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/command", out.str());
}

void Twin::land()
{
    if (!can_fly())
        throw CyberwaveError("land() requires a twin with flight capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;

    const double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << SOURCE_TYPE_TELE << "\",\"command\":\"land\",\"data\":{},\"timestamp\":" << ts
        << "}";
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/command", out.str());
}

void Twin::hover()
{
    if (!can_fly())
        throw CyberwaveError("hover() requires a twin with flight capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;

    const double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << SOURCE_TYPE_TELE << "\",\"command\":\"hover\",\"data\":{},\"timestamp\":" << ts
        << "}";
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/command", out.str());
}

void Twin::grip(double force)
{
    if (!can_grip())
        throw CyberwaveError("grip() requires a twin with gripper capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;

    const double clamped_force = force < 0.0 ? 0.0 : (force > 1.0 ? 1.0 : force);
    const double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << SOURCE_TYPE_TELE << "\",\"command\":\"grip\",\"data\":{\"force\":" << clamped_force
        << "},\"timestamp\":" << ts << "}";
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/command", out.str());
}

void Twin::release()
{
    if (!can_grip())
        throw CyberwaveError("release() requires a twin with gripper capabilities");
    auto mqtt = client().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;

    const double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "{\"source_type\":\"" << SOURCE_TYPE_TELE << "\",\"command\":\"release\",\"data\":{},\"timestamp\":" << ts
        << "}";
    mqtt->publish(mqtt->get_topic_prefix() + "cyberwave/twin/" + uuid() + "/command", out.str());
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
