#include "cyberwave/edge/base_edge_node.h"
#include "cyberwave/client.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>

#include <algorithm>
#include <chrono>
#include <sstream>
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

double timestamp_now()
{
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
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
    web::json::value pos = web::json::value::object();
    pos[from_std("position")] = web::json::value::object();
    pos[from_std("position")][from_std("x")] = x;
    pos[from_std("position")][from_std("y")] = y;
    pos[from_std("position")][from_std("z")] = z;
    pos[from_std("source_type")] = web::json::value::string(from_std(config_.source_type));
    pos[from_std("timestamp")] = timestamp_now();
    mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/position", json_serialize(pos));
    if (rotation_xyzw)
    {
        web::json::value rot = web::json::value::object();
        rot[from_std("rotation")] = web::json::value::object();
        rot[from_std("rotation")][from_std("x")] = (*rotation_xyzw)[0];
        rot[from_std("rotation")][from_std("y")] = (*rotation_xyzw)[1];
        rot[from_std("rotation")][from_std("z")] = (*rotation_xyzw)[2];
        rot[from_std("rotation")][from_std("w")] = (*rotation_xyzw)[3];
        rot[from_std("source_type")] = web::json::value::string(from_std(config_.source_type));
        rot[from_std("timestamp")] = timestamp_now();
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
    j[from_std("source_type")] = web::json::value::string(from_std(config_.source_type));
    j[from_std("timestamp")] = timestamp_now();
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
    j[from_std("action_id")] = web::json::value::string(from_std(action_id));
    j[from_std("status")] = web::json::value::string(from_std(status));
    j[from_std("timestamp")] = timestamp_now();
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
    web::json::value j = web::json::value::object();
    for (const auto& kv : health_data)
        j[from_std(kv.first)] = web::json::value::string(from_std(kv.second));
    j[from_std("edge_uuid")] = web::json::value::string(from_std(config_.edge_uuid));
    j[from_std("timestamp")] = timestamp_now();
    j[from_std("uptime")] = uptime_seconds();
    mqtt->publish(prefix + "cyberwave/twin/" + twin_uuid + "/edge_health", json_serialize(j));
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
    j[from_std("timestamp")] = timestamp_now();
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
    j[from_std("source_type")] = web::json::value::string(from_std(config_.source_type));
    j[from_std("timestamp")] = timestamp_now();
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
