#include "cyberwave/edge/amr_edge_node.h"
#include "cyberwave/client.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>

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

bool parse_position(const web::json::value& j, Position3& out)
{
    if (!j.is_object() || !j.has_field(from_std("position")))
        return false;
    const web::json::value& p = j.at(from_std("position"));
    if (!p.is_object())
        return false;
    if (p.has_field(from_std("x")))
        out.x = p.at(from_std("x")).as_double();
    if (p.has_field(from_std("y")))
        out.y = p.at(from_std("y")).as_double();
    if (p.has_field(from_std("z")))
        out.z = p.at(from_std("z")).as_double();
    return true;
}

bool parse_rotation(const web::json::value& j, RotationQuat& out)
{
    if (!j.is_object() || !j.has_field(from_std("rotation")))
        return false;
    const web::json::value& r = j.at(from_std("rotation"));
    if (!r.is_object())
        return false;
    if (r.has_field(from_std("w")))
        out.w = r.at(from_std("w")).as_double();
    if (r.has_field(from_std("x")))
        out.x = r.at(from_std("x")).as_double();
    if (r.has_field(from_std("y")))
        out.y = r.at(from_std("y")).as_double();
    if (r.has_field(from_std("z")))
        out.z = r.at(from_std("z")).as_double();
    return true;
}

} // namespace

AMREdgeNode::AMREdgeNode(const EdgeNodeConfig& config, Client& client, std::optional<AdapterConfig> adapter_config)
    : BaseEdgeNode(config, client), adapter_config_(std::move(adapter_config).value_or(AdapterConfig::from_env()))
{
}

AMREdgeNode::~AMREdgeNode()
{
    request_shutdown();
    if (adapter_)
    {
        adapter_->set_status_callback(AMRStatusCallback{});
        adapter_->disconnect();
    }
    if (position_thread_.joinable())
        position_thread_.join();
    if (telemetry_thread_.joinable())
        telemetry_thread_.join();
}

void AMREdgeNode::setup()
{
    adapter_ = create_adapter();
    if (adapter_)
    {
        std::weak_ptr<std::atomic<bool>> guard = callback_guard_;
        adapter_->set_status_callback(
            [this, guard](const std::string& action_id, const std::string& status, std::optional<std::string> message,
                          std::optional<double> progress)
            {
                auto alive = guard.lock();
                if (!alive || !alive->load())
                    return;
                on_adapter_status(action_id, status, message, progress);
            });
        connect_adapter();
        position_thread_ = std::thread(&AMREdgeNode::position_loop, this);
        telemetry_thread_ = std::thread(&AMREdgeNode::telemetry_loop, this);
    }
}

void AMREdgeNode::subscribe_to_commands()
{
    std::weak_ptr<std::atomic<bool>> guard = callback_guard_;
    for (const std::string& twin_uuid : get_twin_uuids())
    {
        subscribe_navigate_command(twin_uuid,
                                   [this, guard, twin_uuid](const std::string& payload)
                                   {
                                       auto alive = guard.lock();
                                       if (!alive || !alive->load())
                                           return;
                                       handle_navigate_command(twin_uuid, payload);
                                   });
        subscribe_mission_command(twin_uuid,
                                  [this, guard, twin_uuid](const std::string& payload)
                                  {
                                      auto alive = guard.lock();
                                      if (!alive || !alive->load())
                                          return;
                                      handle_mission_command(twin_uuid, payload);
                                  });
    }
}

void AMREdgeNode::cleanup()
{
    if (adapter_)
    {
        adapter_->set_status_callback(AMRStatusCallback{});
        adapter_->disconnect();
    }
    if (position_thread_.joinable())
    {
        position_thread_.join();
        position_thread_ = std::thread();
    }
    if (telemetry_thread_.joinable())
    {
        telemetry_thread_.join();
        telemetry_thread_ = std::thread();
    }
}

BaseEdgeNode::HealthStatus AMREdgeNode::build_health_status()
{
    HealthStatus h;
    h["adapter_type"] = adapter_config_.adapter_type;
    h["adapter_connected"] = (adapter_ && adapter_->is_connected()) ? "true" : "false";
    h["robot_id"] = adapter_config_.robot_id;
    std::optional<RobotTelemetry> telemetry_snapshot;
    {
        std::lock_guard<std::mutex> lock(telemetry_mutex_);
        telemetry_snapshot = current_telemetry_;
    }
    if (telemetry_snapshot)
    {
        h["robot_state"] = to_string(telemetry_snapshot->state);
        if (telemetry_snapshot->battery_level)
            h["battery_level"] = std::to_string(*telemetry_snapshot->battery_level);
        h["battery_charging"] = telemetry_snapshot->battery_charging ? "true" : "false";
        h["position_rate_hz"] = std::to_string(adapter_config_.position_poll_rate_hz);
    }
    else
    {
        h["robot_state"] = "unknown";
    }
    {
        std::lock_guard<std::mutex> lock(active_actions_mutex_);
        h["active_actions"] = std::to_string(active_actions_.size());
    }
    return h;
}

void AMREdgeNode::connect_adapter()
{
    if (!adapter_)
        return;
    try
    {
        adapter_->connect();
    }
    catch (...)
    {
        // Subclass may override for retry
    }
}

void AMREdgeNode::position_loop()
{
    const double rate = adapter_config_.position_poll_rate_hz > 0.0 ? adapter_config_.position_poll_rate_hz : 10.0;
    const auto interval = std::chrono::duration<double>(1.0 / rate);
    while (running())
    {
        std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval));
        if (!running())
            break;
        if (!adapter_ || !adapter_->is_connected())
            continue;
        try
        {
            std::optional<RobotTelemetry> t = adapter_->poll_telemetry();
            if (t && t->position)
            {
                {
                    std::lock_guard<std::mutex> lock(telemetry_mutex_);
                    current_telemetry_ = t;
                }
                const Position3& p = *t->position;
                std::optional<std::array<double, 4>> rotation_xyzw;
                if (t->rotation)
                {
                    rotation_xyzw = std::array<double, 4>{
                        t->rotation->x,
                        t->rotation->y,
                        t->rotation->z,
                        t->rotation->w,
                    };
                }
                for (const std::string& twin_uuid : get_twin_uuids())
                    publish_position(twin_uuid, p.x, p.y, p.z, rotation_xyzw);
            }
        }
        catch (...)
        {
        }
    }
}

void AMREdgeNode::telemetry_loop()
{
    const double rate = adapter_config_.telemetry_poll_rate_hz > 0.0 ? adapter_config_.telemetry_poll_rate_hz : 1.0;
    const auto interval = std::chrono::duration<double>(1.0 / rate);
    while (running())
    {
        std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval));
        if (!running())
            break;
        if (!adapter_ || !adapter_->is_connected())
            continue;
        try
        {
            std::optional<RobotTelemetry> t = adapter_->poll_telemetry();
            if (t)
            {
                {
                    std::lock_guard<std::mutex> lock(telemetry_mutex_);
                    current_telemetry_ = t;
                }
                std::map<std::string, std::string> data;
                for (const std::string& twin_uuid : get_twin_uuids())
                {
                    if (t->battery_level)
                    {
                        data.clear();
                        data["level"] = std::to_string(*t->battery_level);
                        data["charging"] = t->battery_charging ? "true" : "false";
                        publish_telemetry(twin_uuid, "battery", data);
                    }
                    data.clear();
                    data["state"] = to_string(t->state);
                    for (const auto& kv : t->vendor_data)
                        data["vendor_" + kv.first] = kv.second;
                    publish_event(twin_uuid, "robot_state", data);
                }
            }
        }
        catch (...)
        {
        }
    }
}

void AMREdgeNode::on_adapter_status(const std::string& action_id, const std::string& status,
                                    std::optional<std::string> message, std::optional<double> progress)
{
    std::string twin_uuid;
    {
        std::lock_guard<std::mutex> lock(active_actions_mutex_);
        auto it = active_actions_.find(action_id);
        if (it != active_actions_.end())
        {
            auto& m = it->second;
            m["status"] = status;
            auto tu = m.find("twin_uuid");
            if (tu != m.end())
                twin_uuid = tu->second;
            if (status == "completed" || status == "failed" || status == "cancelled")
                active_actions_.erase(it);
        }
    }
    if (twin_uuid.empty())
    {
        auto uuids = get_twin_uuids();
        if (!uuids.empty())
            twin_uuid = uuids[0];
    }
    if (!twin_uuid.empty())
    {
        publish_nav_status(twin_uuid, action_id, status, std::move(message), progress);
    }
}

void AMREdgeNode::handle_navigate_command(const std::string& twin_uuid, const std::string& json_payload)
{
    if (!adapter_)
        return;
    std::string action_id;
    std::string command = "goto";
    std::optional<Position3> position;
    std::optional<RotationQuat> rotation;
    std::vector<Position3> waypoints;

    try
    {
        web::json::value j = web::json::value::parse(from_std(json_payload));
        if (j.has_field(from_std("action_id")))
            action_id = to_std(j.at(from_std("action_id")).as_string());
        if (action_id.empty())
            action_id =
                "nav-" + std::to_string(static_cast<long>(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (j.has_field(from_std("command")))
            command = to_std(j.at(from_std("command")).as_string());
        if (j.has_field(from_std("position")))
        {
            Position3 p;
            if (parse_position(j, p))
                position = p;
        }
        if (j.has_field(from_std("rotation")))
        {
            RotationQuat r;
            if (parse_rotation(j, r))
                rotation = r;
        }
        if (j.has_field(from_std("waypoints")) && j.at(from_std("waypoints")).is_array())
        {
            for (const auto& wp : j.at(from_std("waypoints")).as_array())
            {
                Position3 p;
                if (wp.is_object() && wp.has_field(from_std("position")))
                {
                    parse_position(wp, p);
                }
                else
                {
                    if (wp.has_field(from_std("x")))
                        p.x = wp.at(from_std("x")).as_double();
                    if (wp.has_field(from_std("y")))
                        p.y = wp.at(from_std("y")).as_double();
                    if (wp.has_field(from_std("z")))
                        p.z = wp.at(from_std("z")).as_double();
                }
                waypoints.push_back(p);
            }
        }
    }
    catch (...)
    {
        publish_nav_status(twin_uuid, action_id.empty() ? "unknown" : action_id, "failed");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(active_actions_mutex_);
        active_actions_[action_id]["twin_uuid"] = twin_uuid;
        active_actions_[action_id]["command"] = command;
        active_actions_[action_id]["status"] = "queued";
    }
    publish_nav_status(twin_uuid, action_id, "queued");

    try
    {
        if (command == "stop")
        {
            adapter_->cancel_navigation(action_id);
        }
        else if (command == "pause")
        {
            adapter_->pause_navigation();
        }
        else if (command == "resume")
        {
            adapter_->resume_navigation();
        }
        else
        {
            bool ok = adapter_->send_navigation_command(action_id, command, position, rotation, waypoints);
            if (!ok)
            {
                publish_nav_status(twin_uuid, action_id, "failed");
                std::lock_guard<std::mutex> lock(active_actions_mutex_);
                active_actions_.erase(action_id);
            }
        }
    }
    catch (...)
    {
        publish_nav_status(twin_uuid, action_id, "failed");
        std::lock_guard<std::mutex> lock(active_actions_mutex_);
        active_actions_.erase(action_id);
    }
}

void AMREdgeNode::handle_mission_command(const std::string& twin_uuid, const std::string& json_payload)
{
    if (!adapter_)
        return;
    std::string command;
    std::string mission_uuid;
    try
    {
        web::json::value j = web::json::value::parse(from_std(json_payload));
        if (j.has_field(from_std("command")))
            command = to_std(j.at(from_std("command")).as_string());
        if (j.has_field(from_std("mission_execution_uuid")))
            mission_uuid = to_std(j.at(from_std("mission_execution_uuid")).as_string());
    }
    catch (...)
    {
        return;
    }
    if (command == "cancel")
    {
        std::vector<std::string> action_ids_to_cancel;
        {
            std::lock_guard<std::mutex> lock(active_actions_mutex_);
            for (const auto& kv : active_actions_)
            {
                auto it = kv.second.find("mission_uuid");
                if (it != kv.second.end() && it->second == mission_uuid)
                    action_ids_to_cancel.push_back(kv.first);
            }
        }
        for (const auto& action_id : action_ids_to_cancel)
        {
            adapter_->cancel_navigation(action_id);
        }
    }
    else if (command == "pause")
    {
        adapter_->pause_navigation();
    }
    else if (command == "resume")
    {
        adapter_->resume_navigation();
    }
}

} // namespace cyberwave
