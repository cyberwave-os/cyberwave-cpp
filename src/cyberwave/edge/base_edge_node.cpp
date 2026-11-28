#include "cyberwave/edge/base_edge_node.h"
#include "cyberwave/client.h"
#include "cyberwave/utils.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <sstream>
#include <string_view>
#include <thread>

namespace cyberwave
{

namespace
{

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

std::string to_std(const utility::string_t& t)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return std::string(t);
#else
    return utility::conversions::to_utf8string(t);
#endif
}

std::string json_serialize(const web::json::value& v)
{
    std::ostringstream os;
    v.serialize(os);
    return os.str();
}

// The source type a node stamps on what it publishes. An empty or unknown
// configured value (a typo in SOURCE_TYPE) becomes `edge`, as Python's
// heartbeat_source_type() does: provenance labelling must not keep a node from
// publishing, and the recording pipeline refuses a source_type it does not know.
std::string stamped_source_type(const std::string& configured)
{
    return is_valid_source_type(configured) ? configured : std::string(SOURCE_TYPE_EDGE);
}

} // namespace

BaseEdgeNode::BaseEdgeNode(const EdgeNodeConfig& config, Client& client)
    : config_(config), client_(client), start_time_(std::chrono::steady_clock::now()),
      callback_guard_(std::make_shared<std::atomic<bool>>(true))
{
}

BaseEdgeNode::~BaseEdgeNode()
{
    request_shutdown();
    if (health_thread_.joinable())
        health_thread_.join();
}

void BaseEdgeNode::run()
{
    config_.validate();
    running_ = true;
    start_time_ = std::chrono::steady_clock::now();
    if (!callback_guard_)
        callback_guard_ = std::make_shared<std::atomic<bool>>(true);
    else
        callback_guard_->store(true);

    try
    {
        discover_twins();
        setup();
        subscribe_to_commands();

        health_thread_ = std::thread(&BaseEdgeNode::health_loop, this);
        main_loop();
    }
    catch (...)
    {
        shutdown();
        throw;
    }
    shutdown();
}

void BaseEdgeNode::request_shutdown() { running_ = false; }

void BaseEdgeNode::shutdown()
{
    running_ = false;
    if (callback_guard_)
    {
        callback_guard_->store(false);
        callback_guard_.reset();
    }
    if (health_thread_.joinable())
    {
        health_thread_.join();
        health_thread_ = std::thread();
    }
    cleanup();
}

std::vector<std::string> BaseEdgeNode::get_twin_uuids() const
{
    if (!discovered_twin_uuids_.empty())
        return discovered_twin_uuids_;
    if (!config_.twin_uuid.empty())
        return {config_.twin_uuid};
    return {};
}

void BaseEdgeNode::discover_twins()
{
    // Default: no REST discovery. Subclass can override and call client_->edges().get(config_.edge_uuid)
    // and populate discovered_twin_uuids_ when Edge exposes twins.
}

void BaseEdgeNode::main_loop()
{
    while (running_)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void BaseEdgeNode::health_loop()
{
    const auto interval =
        std::chrono::seconds(config_.health_publish_interval_sec > 0 ? config_.health_publish_interval_sec : 5);
    while (running_)
    {
        auto slept = std::chrono::steady_clock::duration::zero();
        while (running_ && slept < interval)
        {
            const auto slice = std::min(std::chrono::milliseconds(100),
                                        std::chrono::duration_cast<std::chrono::milliseconds>(interval - slept));
            std::this_thread::sleep_for(slice);
            slept += slice;
        }
        if (!running_)
            break;
        try
        {
            HealthStatus health = build_health_status();
            for (const std::string& uuid : get_twin_uuids())
                publish_health(uuid, health);
        }
        catch (...)
        {
            // Log and continue
        }
    }
}

double BaseEdgeNode::uptime_seconds() const
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time_).count();
}

std::string BaseEdgeNode::topic_prefix_with(const std::shared_ptr<IMqttClient>& mqtt)
{
    if (!mqtt)
        return "";
    return mqtt->get_topic_prefix();
}

void BaseEdgeNode::publish_position(const std::string& twin_uuid, double x, double y, double z,
                                    std::optional<std::array<double, 4>> rotation_xyzw)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string prefix = topic_prefix_with(mqtt);
    const auto source_type = web::json::value::string(from_std(stamped_source_type(config_.source_type)));
    web::json::value pos = web::json::value::object();
    pos[from_std("position")] = web::json::value::object();
    pos[from_std("position")][from_std("x")] = x;
    pos[from_std("position")][from_std("y")] = y;
    pos[from_std("position")][from_std("z")] = z;
    pos[from_std("source_type")] = source_type;
    pos[from_std("timestamp")] = wall_seconds();
    mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/position", json_serialize(pos));
    if (rotation_xyzw)
    {
        web::json::value rot = web::json::value::object();
        rot[from_std("rotation")] = web::json::value::object();
        rot[from_std("rotation")][from_std("x")] = (*rotation_xyzw)[0];
        rot[from_std("rotation")][from_std("y")] = (*rotation_xyzw)[1];
        rot[from_std("rotation")][from_std("z")] = (*rotation_xyzw)[2];
        rot[from_std("rotation")][from_std("w")] = (*rotation_xyzw)[3];
        rot[from_std("source_type")] = source_type;
        rot[from_std("timestamp")] = wall_seconds();
        mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/rotation", json_serialize(rot));
    }
}

void BaseEdgeNode::publish_joint_states(const std::string& twin_uuid, const std::map<std::string, double>& joint_states)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string prefix = topic_prefix_with(mqtt);
    web::json::value j = web::json::value::object();
    web::json::value js = web::json::value::object();
    for (const auto& kv : joint_states)
        js[from_std(kv.first)] = kv.second;
    j[from_std("joint_states")] = js;
    j[from_std("source_type")] = web::json::value::string(from_std(stamped_source_type(config_.source_type)));
    j[from_std("timestamp")] = wall_seconds();
    mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/joint_states", json_serialize(j));
}

void BaseEdgeNode::publish_nav_status(const std::string& twin_uuid, const std::string& action_id,
                                      const std::string& status, std::optional<std::string> message,
                                      std::optional<double> progress)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string prefix = topic_prefix_with(mqtt);
    web::json::value j = web::json::value::object();
    // As Python's publish_nav_status: the backend lets only `edge` (or no
    // source at all) move a physical navigation action, and only
    // `sim`/`sim_tele` move a simulated one. So the leader/follower roles
    // report as `edge`, and `tele`/`edit` send none rather than get the
    // status dropped.
    std::string source_type = stamped_source_type(config_.source_type);
    if (source_type == SOURCE_TYPE_EDGE_LEADER || source_type == SOURCE_TYPE_EDGE_FOLLOWER)
        source_type = SOURCE_TYPE_EDGE;
    if (source_type != SOURCE_TYPE_TELE && source_type != SOURCE_TYPE_EDIT)
        j[from_std("source_type")] = web::json::value::string(from_std(source_type));
    j[from_std("action_id")] = web::json::value::string(from_std(action_id));
    j[from_std("status")] = web::json::value::string(from_std(status));
    j[from_std("timestamp")] = wall_seconds();
    if (message)
        j[from_std("message")] = web::json::value::string(from_std(*message));
    if (progress)
        j[from_std("progress")] = *progress;
    mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/navigate/status", json_serialize(j));
}

void BaseEdgeNode::publish_health(const std::string& twin_uuid, const HealthStatus& health_data)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string prefix = topic_prefix_with(mqtt);
    // The field names the edge_health contract declares. A node's
    // build_health_status() cannot overwrite them; source_type above all: the
    // recording pipeline refuses a heartbeat without one. Built with
    // json_float so the floats are spelled as Python spells them (`5.0`, not `5`).
    static constexpr std::array<std::string_view, 6> contract_fields = {"type",      "source_type", "edge_id",
                                                                        "twin_uuid", "timestamp",   "uptime_seconds"};
    const std::string source_type = stamped_source_type(config_.source_type);
    std::string out = "{\"type\":\"edge_health\",\"source_type\":" + detail::json_quote(source_type) +
                      ",\"edge_id\":" + detail::json_quote(config_.edge_uuid) +
                      ",\"twin_uuid\":" + detail::json_quote(twin_uuid) +
                      ",\"timestamp\":" + detail::json_float(wall_seconds()) +
                      ",\"uptime_seconds\":" + detail::json_float(std::round(uptime_seconds() * 10.0) / 10.0);
    for (const auto& kv : health_data)
    {
        if (std::find(contract_fields.begin(), contract_fields.end(), kv.first) != contract_fields.end())
            continue;
        out += "," + detail::json_quote(kv.first) + ":" + detail::json_quote(kv.second);
    }
    out += "}";
    mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/edge_health", out);
}

void BaseEdgeNode::publish_gps(const std::string& twin_uuid, const GpsFix& fix)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string prefix = topic_prefix_with(mqtt);
    web::json::value j = web::json::value::object();
    j[from_std("source_type")] = web::json::value::string(from_std(stamped_source_type(config_.source_type)));
    j[from_std("latitude")] = fix.latitude;
    j[from_std("longitude")] = fix.longitude;
    j[from_std("altitude")] = fix.altitude;
    j[from_std("timestamp")] = wall_seconds();
    if (fix.satellite_count)
        j[from_std("satellite_count")] = *fix.satellite_count;
    if (fix.signal_level)
        j[from_std("signal_level")] = *fix.signal_level;
    if (fix.compass_heading)
        j[from_std("compass_heading")] = *fix.compass_heading;
    if (fix.horizontal_accuracy)
        j[from_std("horizontal_accuracy")] = *fix.horizontal_accuracy;
    if (fix.vertical_accuracy)
        j[from_std("vertical_accuracy")] = *fix.vertical_accuracy;
    if (fix.fix_type)
        j[from_std("fix_type")] = web::json::value::string(from_std(*fix.fix_type));
    mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/gps", json_serialize(j));
}

void BaseEdgeNode::publish_event(const std::string& twin_uuid, const std::string& event_type,
                                 const std::map<std::string, std::string>& data)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string prefix = topic_prefix_with(mqtt);
    web::json::value j = web::json::value::object();
    j[from_std("event_type")] = web::json::value::string(from_std(event_type));
    j[from_std("source")] = web::json::value::string(from_std("edge_node"));
    j[from_std("source_type")] = web::json::value::string(from_std(stamped_source_type(config_.source_type)));
    j[from_std("timestamp")] = wall_seconds();
    web::json::value d = web::json::value::object();
    for (const auto& kv : data)
        d[from_std(kv.first)] = web::json::value::string(from_std(kv.second));
    j[from_std("data")] = d;
    mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/event", json_serialize(j));
}

void BaseEdgeNode::publish_telemetry(const std::string& twin_uuid, const std::string& telemetry_type,
                                     const std::map<std::string, std::string>& data)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt || !mqtt->is_connected())
        return;
    std::string prefix = topic_prefix_with(mqtt);
    web::json::value j = web::json::value::object();
    j[from_std("telemetry_type")] = web::json::value::string(from_std(telemetry_type));
    j[from_std("source_type")] = web::json::value::string(from_std(stamped_source_type(config_.source_type)));
    j[from_std("timestamp")] = wall_seconds();
    web::json::value d = web::json::value::object();
    for (const auto& kv : data)
        d[from_std(kv.first)] = web::json::value::string(from_std(kv.second));
    j[from_std("data")] = d;
    mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/telemetry", json_serialize(j));
}

void BaseEdgeNode::subscribe_navigate_command(const std::string& twin_uuid, CommandHandler handler)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt)
        return;
    std::string prefix = topic_prefix_with(mqtt);
    mqtt->subscribe(prefix + "cyberwave/twin/" + twin_uuid + "/navigate/command", std::move(handler));
}

void BaseEdgeNode::subscribe_motion_command(const std::string& twin_uuid, CommandHandler handler)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt)
        return;
    std::string prefix = topic_prefix_with(mqtt);
    mqtt->subscribe(prefix + "cyberwave/twin/" + twin_uuid + "/motion/command", std::move(handler));
}

void BaseEdgeNode::subscribe_mission_command(const std::string& twin_uuid, CommandHandler handler)
{
    auto mqtt = client_.get().mqtt_client();
    if (!mqtt)
        return;
    std::string prefix = topic_prefix_with(mqtt);
    mqtt->subscribe(prefix + "cyberwave/twin/" + twin_uuid + "/mission/command", std::move(handler));
}

} // namespace cyberwave
