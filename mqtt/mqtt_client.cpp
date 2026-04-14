#include "mqtt_client.h"
#include "constants.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string_view>

using json = nlohmann::json;

namespace cyberwave
{
namespace
{

inline spdlog::logger& mqtt_log()
{
    if (auto l = spdlog::get("cyberwave.mqtt"); l)
    {
        return *l;
    }
    return *spdlog::stdout_color_mt("cyberwave.mqtt");
}

double now_seconds()
{
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string read_env(const char* name)
{
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

int parse_int_env(const std::string& value, int fallback)
{
    if (value.empty())
    {
        return fallback;
    }
    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        mqtt_log().warn("Invalid integer env value '{}', using fallback {}", value, fallback);
        return fallback;
    }
}

bool contains_uuid(const std::vector<std::string>& items, const std::string& value)
{
    return std::find(items.begin(), items.end(), value) != items.end();
}

bool has_nonempty_env(const char* name)
{
    const char* value = std::getenv(name);
    return value && value[0] != '\0';
}

bool parse_bool_env(const std::string& value)
{
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value)
    {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
    {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
    {
        return false;
    }

    mqtt_log().warn("Invalid boolean env value '{}', keeping configured TLS setting", value);
    throw std::invalid_argument("invalid boolean env");
}

} // namespace

CyberwaveMQTTClient::CyberwaveMQTTClient(const CyberwaveConfig& config) : config_(config), connected_(false)
{
    const std::string env_mqtt_host = read_env("CYBERWAVE_MQTT_HOST");
    mqtt_broker_ =
        config.mqtt_host.empty() ? (env_mqtt_host.empty() ? "mqtt.cyberwave.com" : env_mqtt_host) : config.mqtt_host;

    const int env_mqtt_port = parse_int_env(read_env("CYBERWAVE_MQTT_PORT"), 1883);
    mqtt_port_ = config.mqtt_port > 0 ? config.mqtt_port : env_mqtt_port;

    const std::string env_mqtt_username = read_env("CYBERWAVE_MQTT_USERNAME");
    const std::string env_mqtt_user = read_env("CYBERWAVE_MQTT_USER");
    const std::string resolved_env_username = env_mqtt_username.empty() ? env_mqtt_user : env_mqtt_username;
    mqtt_username_ = config.mqtt_username.empty() ? (env_mqtt_username.empty() ? "mqttcyb" : env_mqtt_username)
                                                  : config.mqtt_username;
    if (config.mqtt_username.empty())
    {
        mqtt_username_ = resolved_env_username.empty() ? "mqttcyb" : resolved_env_username;
    }
    mqtt_api_token_ = !config.mqtt_api_token.empty() ? config.mqtt_api_token : config.mqtt_password;
    if (mqtt_api_token_.empty())
    {
        const std::string env_token = read_env("CYBERWAVE_MQTT_API_TOKEN");
        const std::string env_password = read_env("CYBERWAVE_MQTT_PASSWORD");
        mqtt_api_token_ =
            env_token.empty() ? (env_password.empty() ? read_env("CYBERWAVE_API_KEY") : env_password) : env_token;
    }
    if (mqtt_api_token_.empty())
    {
        throw std::invalid_argument("MQTT API token is required (set mqtt_api_token / CYBERWAVE_API_KEY)");
    }

    const std::string env_source_type = read_env("CYBERWAVE_SOURCE_TYPE");
    source_type_ = config.source_type.empty() ? (env_source_type.empty() ? SOURCE_TYPE_EDGE : env_source_type)
                                              : config.source_type;
    if (!is_valid_source_type(source_type_))
    {
        source_type_ = SOURCE_TYPE_EDGE;
    }
    topic_prefix_ = resolve_topic_prefix(config);

    bool use_tls = config_.mqtt_use_tls;
    if (has_nonempty_env("CYBERWAVE_MQTT_USE_TLS"))
    {
        const std::string env_mqtt_tls = read_env("CYBERWAVE_MQTT_USE_TLS");
        try
        {
            use_tls = parse_bool_env(env_mqtt_tls);
        }
        catch (const std::invalid_argument&)
        {
        }
    }

    if (config_.mqtt_tls_ca_cert.empty())
    {
        config_.mqtt_tls_ca_cert = read_env("CYBERWAVE_MQTT_TLS_CA_CERT");
    }

    const std::string server_uri = (use_tls ? "ssl://" : "tcp://") + mqtt_broker_ + ":" + std::to_string(mqtt_port_);
    client_id_ = (config_.runtime_mode == "simulation" ? "sdk_sim_" : "sdk_") + generate_client_id();
    client_ = std::make_unique<mqtt::async_client>(server_uri, client_id_);

    callback_ = std::make_unique<MQTTCallback>(*this);
    client_->set_callback(*callback_);
}

CyberwaveMQTTClient::~CyberwaveMQTTClient() { disconnect(); }

std::string CyberwaveMQTTClient::generate_client_id()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 999999);
    return std::to_string(dis(gen));
}

std::string CyberwaveMQTTClient::normalize_topic_prefix(const std::string& raw_prefix)
{
    if (raw_prefix.empty())
    {
        return "";
    }

    std::string normalized = raw_prefix;
    const std::string::size_type first_non_space = normalized.find_first_not_of(" \t\n\r");
    if (first_non_space == std::string::npos)
    {
        return "";
    }
    normalized.erase(0, first_non_space);
    const std::string::size_type last_non_space = normalized.find_last_not_of(" \t\n\r");
    if (last_non_space != std::string::npos)
    {
        normalized.erase(last_non_space + 1);
    }
    if (normalized.empty())
    {
        return "";
    }
    if (normalized.size() >= 2 && ((normalized.front() == '"' && normalized.back() == '"') ||
                                   (normalized.front() == '\'' && normalized.back() == '\'')))
    {
        normalized = normalized.substr(1, normalized.size() - 2);
    }
    if (normalized.empty())
    {
        return "";
    }
    if (normalized == "production" || normalized == "PRODUCTION" || normalized == "Production")
    {
        return "";
    }
    return normalized;
}

std::string CyberwaveMQTTClient::resolve_topic_prefix(const CyberwaveConfig& config) const
{
    if (!config.topic_prefix.empty())
    {
        return normalize_topic_prefix(config.topic_prefix);
    }

    const std::string explicit_prefix = read_env("CYBERWAVE_MQTT_TOPIC_PREFIX");
    if (!explicit_prefix.empty())
    {
        return normalize_topic_prefix(explicit_prefix);
    }

    const std::string prefixed_environment = read_env("CYBERWAVE_ENVIRONMENT");
    if (!prefixed_environment.empty())
    {
        return normalize_topic_prefix(prefixed_environment);
    }

    // Legacy fallback.
    return normalize_topic_prefix(read_env("ENVIRONMENT"));
}

std::string CyberwaveMQTTClient::with_prefix(const std::string& topic_suffix) const
{
    return topic_prefix_ + topic_suffix;
}

bool CyberwaveMQTTClient::parse_bool_env(const std::string& value) const
{
    std::string normalized;
    normalized.reserve(value.size());
    for (char c : value)
    {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

bool CyberwaveMQTTClient::is_valid_source_type(const std::string& source_type) const
{
    return source_type == SOURCE_TYPE_EDGE || source_type == SOURCE_TYPE_EDGE_LEADER ||
           source_type == SOURCE_TYPE_EDGE_FOLLOWER || source_type == SOURCE_TYPE_TELE ||
           source_type == SOURCE_TYPE_EDIT || source_type == SOURCE_TYPE_SIM || source_type == SOURCE_TYPE_SIM_TELE;
}

void CyberwaveMQTTClient::connect()
{
    if (connected_)
    {
        return;
    }

    const bool use_tls = config_.mqtt_use_tls;
    try
    {
        mqtt::connect_options conn_opts;
        conn_opts.set_user_name(mqtt_username_);
        conn_opts.set_password(mqtt_api_token_);
        conn_opts.set_keep_alive_interval(60);
        conn_opts.set_clean_session(true);
        conn_opts.set_automatic_reconnect(1, 30);
        if (config_.mqtt_protocol > 0)
        {
            conn_opts.set_mqtt_version(config_.mqtt_protocol);
        }
        if (use_tls)
        {
            mqtt::ssl_options_builder ssl_builder;
            ssl_builder.enable_server_cert_auth(true);
            if (!config_.mqtt_tls_ca_cert.empty())
            {
                bool ca_exists = false;
                try
                {
                    ca_exists = std::filesystem::exists(config_.mqtt_tls_ca_cert);
                }
                catch (...)
                {
                    ca_exists = false;
                }

                mqtt_log().info("MQTT TLS CA cert configured: '{}' (exists={})", config_.mqtt_tls_ca_cert, ca_exists);

                if (ca_exists)
                {
                    ssl_builder.trust_store(config_.mqtt_tls_ca_cert);
                }
                else
                {
                    mqtt_log().warn("MQTT TLS CA cert path not found; falling back to system trust store: '{}'",
                                    config_.mqtt_tls_ca_cert);
                }
            }
            else
            {
                // Some MQTT TLS stacks do not automatically load the platform trust store unless
                // an explicit trust store is set. Load common CA bundle paths if present.
                const char* default_ca_paths[] = {
                    "/etc/ssl/certs/ca-certificates.crt",
                    "/etc/ssl/cert.pem",
                };

                bool loaded_default_ca = false;
                for (const char* path : default_ca_paths)
                {
                    if (!path)
                        continue;
                    try
                    {
                        if (std::filesystem::exists(path))
                        {
                            ssl_builder.trust_store(path);
                            loaded_default_ca = true;
                            mqtt_log().info("MQTT TLS CA cert not configured; using default CA bundle: '{}'", path);
                            break;
                        }
                    }
                    catch (...)
                    {
                        // Ignore and try next path.
                    }
                }
                if (!loaded_default_ca)
                {
                    mqtt_log().warn("MQTT TLS CA cert not configured and default CA bundles not found; relying on MQTT "
                                    "TLS defaults");
                }
            }
            conn_opts.set_ssl(ssl_builder.finalize());
        }

        mqtt_log().info("Connecting to MQTT broker: {}:{} ({})", mqtt_broker_, mqtt_port_,
                        use_tls ? "TLS enabled" : "TLS disabled");

        auto token = client_->connect(conn_opts);
        if (!token->wait_for(std::chrono::seconds(10)))
        {
            throw std::runtime_error("MQTT connect timed out after 10s");
        }

        connected_ = true;
        mqtt_log().info("Connected to MQTT broker successfully");
        resubscribe_registered_topics();

        std::lock_guard<std::mutex> lock(telemetry_mutex_);
        for (const auto& twin_uuid : twin_uuids_)
        {
            if (!contains_uuid(twin_uuids_with_telemetry_start_, twin_uuid))
            {
                twin_uuids_with_telemetry_start_.push_back(twin_uuid);
                publish_connect_message(twin_uuid);
                publish_telemetry_start_message(twin_uuid);
            }
        }
    }
    catch (const mqtt::exception& exc)
    {
        std::string message = std::string("MQTT connection error: ") + exc.what();
        if (mqtt_port_ == 1883 && use_tls)
        {
            message += " Hint: port 1883 is usually non-TLS. Set CYBERWAVE_MQTT_USE_TLS=false, "
                       "or use port 8883 for TLS.";
        }
        mqtt_log().error("{}", message);
        throw std::runtime_error(message);
    }
}

void CyberwaveMQTTClient::disconnect()
{
    if (!connected_)
    {
        return;
    }

    try
    {
        std::vector<std::string> tracked_twins;
        {
            std::lock_guard<std::mutex> lock(telemetry_mutex_);
            tracked_twins = twin_uuids_;
        }
        for (const auto& twin_uuid : tracked_twins)
        {
            publish_disconnect_message(twin_uuid);
            publish_telemetry_end(twin_uuid);
        }

        auto token = client_->disconnect();
        if (!token->wait_for(std::chrono::seconds(5)))
        {
            mqtt_log().warn("MQTT disconnect timed out after 5s; forcing local disconnect state");
        }
        connected_ = false;
        mqtt_log().info("Disconnected from MQTT broker");
    }
    catch (const mqtt::exception& exc)
    {
        mqtt_log().error("MQTT disconnect error: {}", exc.what());
    }
}

bool CyberwaveMQTTClient::is_connected() const { return connected_; }

std::string CyberwaveMQTTClient::get_topic_prefix() const { return topic_prefix_; }

void CyberwaveMQTTClient::publish_connect_message(const std::string& twin_uuid)
{
    publish(with_prefix("cyberwave/twin/" + twin_uuid + "/telemetry"),
            json{{"type", "connected"}, {"timestamp", now_seconds()}});
}

void CyberwaveMQTTClient::publish_disconnect_message(const std::string& twin_uuid)
{
    publish(with_prefix("cyberwave/twin/" + twin_uuid + "/telemetry"),
            json{{"type", "disconnected"}, {"timestamp", now_seconds()}});
}

void CyberwaveMQTTClient::handle_twin_update_with_telemetry(const std::string& twin_uuid, const json& metadata)
{
    std::lock_guard<std::mutex> lock(telemetry_mutex_);
    if (!contains_uuid(twin_uuids_, twin_uuid))
    {
        twin_uuids_.push_back(twin_uuid);
    }
    if (!contains_uuid(twin_uuids_with_telemetry_start_, twin_uuid))
    {
        twin_uuids_with_telemetry_start_.push_back(twin_uuid);
        publish_connect_message(twin_uuid);
        publish_telemetry_start_message(twin_uuid, metadata);
    }
}

void CyberwaveMQTTClient::publish_telemetry_start_message(const std::string& twin_uuid, const json& metadata)
{
    json message = {{"type", "telemetry_start"}, {"timestamp", now_seconds()}};
    if (metadata.is_object())
    {
        if (metadata.contains("fps"))
        {
            message["fps"] = metadata.at("fps");
        }
        if (metadata.contains("observations"))
        {
            message["observations"] = metadata.at("observations");
        }
    }
    publish(with_prefix("cyberwave/twin/" + twin_uuid + "/telemetry"), message);
}

void CyberwaveMQTTClient::publish_telemetry_start(const std::string& twin_uuid, const json& metadata)
{
    handle_twin_update_with_telemetry(twin_uuid, metadata);
}

void CyberwaveMQTTClient::publish_telemetry_end(const std::string& twin_uuid)
{
    publish(with_prefix("cyberwave/twin/" + twin_uuid + "/telemetry"),
            json{{"type", "telemetry_end"}, {"timestamp", now_seconds()}});

    std::lock_guard<std::mutex> lock(telemetry_mutex_);
    auto it = std::find(twin_uuids_with_telemetry_start_.begin(), twin_uuids_with_telemetry_start_.end(), twin_uuid);
    if (it != twin_uuids_with_telemetry_start_.end())
    {
        twin_uuids_with_telemetry_start_.erase(it);
    }
}

void CyberwaveMQTTClient::publish_connected(const std::string& twin_uuid) { publish_connect_message(twin_uuid); }

void CyberwaveMQTTClient::publish_disconnected(const std::string& twin_uuid) { publish_disconnect_message(twin_uuid); }

void CyberwaveMQTTClient::publish_initial_observation(const std::string& twin_uuid, const json& observations,
                                                      double fps)
{
    bool already_started = false;
    {
        std::lock_guard<std::mutex> lock(telemetry_mutex_);
        already_started = contains_uuid(twin_uuids_with_telemetry_start_, twin_uuid);
    }

    if (!already_started)
    {
        publish_telemetry_start(twin_uuid, json{{"fps", fps}, {"observations", observations}});
        return;
    }
    publish(with_prefix("cyberwave/twin/" + twin_uuid + "/telemetry"), json{{"type", "initial_observation"},
                                                                            {"observations", observations},
                                                                            {"fps", fps},
                                                                            {"timestamp", now_seconds()}});
}

SubscriptionId CyberwaveMQTTClient::subscribe_with_id(const std::string& topic, MessageCallback callback, int qos)
{
    SubscriptionId subscription_id = 0;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        subscription_id = next_subscription_id_++;
        message_callbacks_[topic].push_back({subscription_id, std::move(callback)});
        const auto qos_it = subscription_qos_.find(topic);
        if (qos_it == subscription_qos_.end())
        {
            subscription_qos_[topic] = qos;
        }
        else if (qos > qos_it->second)
        {
            subscription_qos_[topic] = qos;
        }
    }

    if (!connected_)
    {
        mqtt_log().warn("Cannot subscribe to {}: not connected to MQTT broker", topic);
        return subscription_id;
    }

    try
    {
        auto token = client_->subscribe(topic, qos);
        if (!token->wait_for(std::chrono::seconds(5)))
        {
            mqtt_log().warn("MQTT subscribe timed out after 5s: {} (continuing; broker may still complete "
                            "subscription asynchronously)",
                            topic);
        }
        mqtt_log().info("Subscribed to topic: {}", topic);
    }
    catch (const mqtt::exception& exc)
    {
        mqtt_log().error("MQTT subscribe error: {}", exc.what());
        return subscription_id;
    }

    return subscription_id;
}

void CyberwaveMQTTClient::subscribe(const std::string& topic, MessageCallback callback, int qos)
{
    (void)subscribe_with_id(topic, std::move(callback), qos);
}

void CyberwaveMQTTClient::subscribe(const std::string& topic, std::function<void(const json& message)> callback,
                                    int qos)
{
    subscribe(
        topic, [callback = std::move(callback)](const std::string&, const json& message) { callback(message); }, qos);
}

void CyberwaveMQTTClient::unsubscribe(SubscriptionId subscription_id)
{
    std::vector<std::string> topics_to_unsubscribe;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        for (auto callbacks_it = message_callbacks_.begin(); callbacks_it != message_callbacks_.end();)
        {
            auto& callbacks = callbacks_it->second;
            callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(),
                                           [subscription_id](const RegisteredCallback& registered)
                                           { return registered.id == subscription_id; }),
                            callbacks.end());

            if (callbacks.empty())
            {
                topics_to_unsubscribe.push_back(callbacks_it->first);
                subscription_qos_.erase(callbacks_it->first);
                callbacks_it = message_callbacks_.erase(callbacks_it);
            }
            else
            {
                ++callbacks_it;
            }
        }
    }

    if (!connected_)
    {
        return;
    }

    for (const auto& topic : topics_to_unsubscribe)
    {
        try
        {
            auto token = client_->unsubscribe(topic);
            if (!token->wait_for(std::chrono::seconds(5)))
            {
                mqtt_log().warn("MQTT unsubscribe timed out after 5s: {}", topic);
            }
        }
        catch (const mqtt::exception& exc)
        {
            mqtt_log().error("MQTT unsubscribe error: {}", exc.what());
        }
    }
}

void CyberwaveMQTTClient::publish(const std::string& topic, const json& message, int qos)
{
    if (!connected_)
    {
        mqtt_log().warn("Cannot publish to {}: not connected to MQTT broker", topic);
        return;
    }

    try
    {
        json payload_json = message;
        if (payload_json.is_object() && !payload_json.contains("session_id"))
        {
            payload_json["session_id"] = client_id_;
        }
        std::string payload = payload_json.dump();
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(qos);
        auto token = client_->publish(msg);
        // QoS 0 is fire-and-forget; waiting can introduce artificial backpressure
        // and timeout noise under high frame rates.
        if (qos <= 0)
        {
            return;
        }
        if (!token->wait_for(std::chrono::seconds(5)))
        {
            mqtt_log().warn("MQTT publish timed out after 5s: {}", topic);
        }
    }
    catch (const mqtt::exception& exc)
    {
        mqtt_log().error("MQTT publish error: {}", exc.what());
        return;
    }
}

void CyberwaveMQTTClient::publish(const std::string& topic, const std::string& message, int qos)
{
    if (!connected_)
    {
        mqtt_log().warn("Cannot publish to {}: not connected to MQTT broker", topic);
        return;
    }

    try
    {
        auto msg = mqtt::make_message(topic, message);
        msg->set_qos(qos);
        auto token = client_->publish(msg);
        if (qos <= 0)
        {
            return;
        }
        if (!token->wait_for(std::chrono::seconds(5)))
        {
            mqtt_log().warn("MQTT publish timed out after 5s: {}", topic);
        }
    }
    catch (const mqtt::exception& exc)
    {
        mqtt_log().error("MQTT publish error: {}", exc.what());
    }
}

void CyberwaveMQTTClient::subscribe_twin_position(const std::string& twin_uuid, MessageCallback callback)
{
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/position"), callback);
}

void CyberwaveMQTTClient::subscribe_twin_rotation(const std::string& twin_uuid, MessageCallback callback)
{
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/rotation"), callback);
}

void CyberwaveMQTTClient::subscribe_joint_states(const std::string& twin_uuid, MessageCallback callback)
{
    subscribe(with_prefix("cyberwave/joint/" + twin_uuid + "/+"), callback);
}

void CyberwaveMQTTClient::subscribe_twin(const std::string& twin_uuid, MessageCallback callback)
{
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/+"), callback);
}

void CyberwaveMQTTClient::update_twin_position(const std::string& twin_uuid, const Position& position)
{
    handle_twin_update_with_telemetry(twin_uuid);
    const std::string topic = with_prefix("cyberwave/twin/" + twin_uuid + "/position");
    const json message = {
        {"source_type", source_type_},
        {"type", "position"},
        {"position", {{"x", position.x}, {"y", position.y}, {"z", position.z}}},
        {"timestamp", now_seconds()},
    };
    publish(topic, message);
}

void CyberwaveMQTTClient::publish_twin_position(const std::string& twin_uuid, double x, double y, double z)
{
    update_twin_position(twin_uuid, Position{.x = x, .y = y, .z = z});
}

void CyberwaveMQTTClient::update_twin_rotation(const std::string& twin_uuid, const Rotation& rotation)
{
    handle_twin_update_with_telemetry(twin_uuid);
    const std::string topic = with_prefix("cyberwave/twin/" + twin_uuid + "/rotation");
    const json message = {
        {"source_type", source_type_},
        {"type", "rotation"},
        {"rotation", {{"x", rotation.x}, {"y", rotation.y}, {"z", rotation.z}, {"w", rotation.w}}},
        {"timestamp", now_seconds()},
    };
    publish(topic, message);
}

void CyberwaveMQTTClient::update_twin_scale(const std::string& twin_uuid, const Scale& scale)
{
    handle_twin_update_with_telemetry(twin_uuid);
    const std::string topic = with_prefix("cyberwave/twin/" + twin_uuid + "/scale");
    const json message = {
        {"source_type", source_type_},
        {"type", "scale"},
        {"scale", {{"x", scale.x}, {"y", scale.y}, {"z", scale.z}}},
        {"timestamp", now_seconds()},
    };
    publish(topic, message);
}

void CyberwaveMQTTClient::update_joint_state(const std::string& twin_uuid, const std::string& joint_name,
                                             const JointState& state, const std::string& source_type)
{
    update_joint_state(twin_uuid, joint_name, state, -1.0, source_type);
}

void CyberwaveMQTTClient::update_joint_state(const std::string& twin_uuid, const std::string& joint_name,
                                             const JointState& state, double timestamp, const std::string& source_type)
{
    const std::string effective_source_type = source_type.empty() ? source_type_ : source_type;
    if (!is_valid_source_type(effective_source_type))
    {
        throw std::invalid_argument("Invalid source_type for update_joint_state: " + effective_source_type);
    }

    handle_twin_update_with_telemetry(twin_uuid);
    const std::string topic = with_prefix("cyberwave/joint/" + twin_uuid + "/update");
    json joint_state = json::object();
    if (state.position.has_value())
    {
        joint_state["position"] = state.position.value();
    }
    if (state.velocity.has_value())
    {
        joint_state["velocity"] = state.velocity.value();
    }
    if (state.effort.has_value())
    {
        joint_state["effort"] = state.effort.value();
    }

    const json message = {
        {"source_type", effective_source_type},
        {"type", "joint_state"},
        {"joint_name", joint_name},
        {"joint_state", joint_state},
        {"timestamp", timestamp >= 0.0 ? timestamp : now_seconds()},
    };
    publish(topic, message);
}

void CyberwaveMQTTClient::update_joint_states(const std::string& twin_uuid,
                                              const std::map<std::string, JointState>& joints,
                                              const std::string& source_type)
{
    if (!is_valid_source_type(source_type.empty() ? source_type_ : source_type))
    {
        throw std::invalid_argument("Invalid source_type for update_joint_states: " +
                                    (source_type.empty() ? source_type_ : source_type));
    }
    for (const auto& pair : joints)
    {
        try
        {
            update_joint_state(twin_uuid, pair.first, pair.second, source_type);
        }
        catch (const std::exception& e)
        {
            mqtt_log().error("Failed to update joint '{}': {}", pair.first, e.what());
        }
    }
}

void CyberwaveMQTTClient::update_joints_state(const std::string& twin_uuid,
                                              const std::map<std::string, double>& joint_positions,
                                              const std::string& source_type)
{
    update_joints_state(twin_uuid, joint_positions, source_type, {}, {}, -1.0, "", "", "");
}

void CyberwaveMQTTClient::update_joints_state(
    const std::string& twin_uuid, const std::map<std::string, double>& joint_positions, const std::string& source_type,
    const std::map<std::string, double>& velocities, const std::map<std::string, double>& efforts, double timestamp,
    const std::string& workload_uuid, const std::string& session_id, const std::string& source_subtype)
{
    const std::string effective_source_type = source_type.empty() ? source_type_ : source_type;
    if (!is_valid_source_type(effective_source_type))
    {
        throw std::invalid_argument("Invalid source_type for update_joints_state: " + effective_source_type);
    }
    if (joint_positions.empty())
    {
        throw std::invalid_argument("joint_positions cannot be empty");
    }

    handle_twin_update_with_telemetry(twin_uuid);
    const bool use_aggregated = !velocities.empty() || !efforts.empty() || timestamp >= 0.0 || !workload_uuid.empty() ||
                                !session_id.empty() || !source_subtype.empty();
    json message = {{"source_type", effective_source_type}};
    if (use_aggregated)
    {
        message["positions"] = json::object();
        for (const auto& [joint_name, joint_position] : joint_positions)
            message["positions"][joint_name] = joint_position;

        if (!velocities.empty())
        {
            message["velocities"] = json::object();
            for (const auto& [joint_name, joint_velocity] : velocities)
                message["velocities"][joint_name] = joint_velocity;
        }
        if (!efforts.empty())
        {
            message["efforts"] = json::object();
            for (const auto& [joint_name, joint_effort] : efforts)
                message["efforts"][joint_name] = joint_effort;
        }
        if (timestamp >= 0.0)
            message["timestamp"] = timestamp;
        if (!workload_uuid.empty())
            message["workload_uuid"] = workload_uuid;
        if (!session_id.empty())
            message["session_id"] = session_id;
        if (!source_subtype.empty())
            message["source_subtype"] = source_subtype;
    }
    else
    {
        for (const auto& [joint_name, joint_position] : joint_positions)
            message[joint_name] = joint_position;
    }
    publish(with_prefix("cyberwave/joint/" + twin_uuid + "/update"), message);
}

void CyberwaveMQTTClient::subscribe_environment(const std::string& environment_uuid, MessageCallback callback)
{
    subscribe(with_prefix("cyberwave/environment/" + environment_uuid + "/+"), callback);
}

void CyberwaveMQTTClient::publish_environment_update(const std::string& environment_uuid,
                                                     const std::string& update_type, const json& data)
{
    const std::string topic = with_prefix("cyberwave/environment/" + environment_uuid + "/" + update_type);
    const json message = {
        {"type", update_type},
        {"data", data},
        {"timestamp", now_seconds()},
    };
    publish(topic, message);
}

void CyberwaveMQTTClient::subscribe_video_stream(const std::string& twin_uuid, MessageCallback callback)
{
    handle_twin_update_with_telemetry(twin_uuid);
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/video"), callback);
}

void CyberwaveMQTTClient::subscribe_depth_stream(const std::string& twin_uuid, MessageCallback callback)
{
    handle_twin_update_with_telemetry(twin_uuid);
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/depth"), callback);
}

void CyberwaveMQTTClient::subscribe_pointcloud_stream(const std::string& twin_uuid, MessageCallback callback)
{
    handle_twin_update_with_telemetry(twin_uuid);
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/pointcloud"), callback);
}

void CyberwaveMQTTClient::publish_depth_frame(const std::string& twin_uuid, const json& depth_data)
{
    publish_depth_frame(twin_uuid, depth_data, now_seconds());
}

void CyberwaveMQTTClient::publish_depth_frame(const std::string& twin_uuid, const json& depth_data, double timestamp)
{
    handle_twin_update_with_telemetry(twin_uuid);
    const std::string topic = with_prefix("cyberwave/twin/" + twin_uuid + "/depth");
    const json message = {
        {"type", "depth_data"},
        {"data", depth_data},
        {"timestamp", timestamp},
    };
    publish(topic, message);
}

void CyberwaveMQTTClient::publish_webrtc_message(const std::string& twin_uuid, const json& webrtc_data)
{
    handle_twin_update_with_telemetry(twin_uuid);
    std::string topic = with_prefix("cyberwave/twin/" + twin_uuid + "/webrtc");
    if (webrtc_data.is_object() && webrtc_data.contains("type") && webrtc_data.at("type").is_string())
    {
        const std::string message_type = webrtc_data.at("type").get<std::string>();
        if (message_type == "offer")
        {
            topic = with_prefix("cyberwave/twin/" + twin_uuid + "/webrtc-offer");
        }
        else if (message_type == "answer")
        {
            topic = with_prefix("cyberwave/twin/" + twin_uuid + "/webrtc-answer");
        }
        else if (message_type == "candidate")
        {
            topic = with_prefix("cyberwave/twin/" + twin_uuid + "/webrtc-candidate");
        }
    }
    publish(topic, webrtc_data);
}

void CyberwaveMQTTClient::subscribe_webrtc_messages(const std::string& twin_uuid, MessageCallback callback)
{
    handle_twin_update_with_telemetry(twin_uuid);
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/webrtc-offer"), callback);
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/webrtc-answer"), callback);
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/webrtc-candidate"), callback);
}

void CyberwaveMQTTClient::publish_command_message(const std::string& twin_uuid, const std::string& status)
{
    publish_command_message(twin_uuid, json{{"status", status}});
}

void CyberwaveMQTTClient::publish_command_message(const std::string& twin_uuid, const json& status_payload)
{
    publish(with_prefix("cyberwave/twin/" + twin_uuid + "/command"), status_payload);
}

void CyberwaveMQTTClient::subscribe_command_message(const std::string& twin_uuid, MessageCallback callback)
{
    subscribe(with_prefix("cyberwave/twin/" + twin_uuid + "/command"), callback);
}

void CyberwaveMQTTClient::ping(const std::string& resource_uuid)
{
    publish(with_prefix("cyberwave/ping/" + resource_uuid + "/request"),
            json{{"type", "ping"}, {"timestamp", now_seconds()}});
}

void CyberwaveMQTTClient::subscribe_pong(const std::string& resource_uuid, MessageCallback callback)
{
    subscribe(with_prefix("cyberwave/pong/" + resource_uuid + "/response"), callback);
}

void CyberwaveMQTTClient::handle_message(const std::string& topic, const std::string& payload)
{
    json parsed_message;
    try
    {
        parsed_message = json::parse(payload);
    }
    catch (const json::exception& e)
    {
        mqtt_log().error("JSON parse error for topic {}: {}", topic, e.what());
        parsed_message = payload;
    }

    std::vector<MessageCallback> matching_callbacks;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);

        auto exact_it = message_callbacks_.find(topic);
        if (exact_it != message_callbacks_.end())
        {
            for (const auto& callback : exact_it->second)
            {
                matching_callbacks.push_back(callback.callback);
            }
        }

        for (const auto& [pattern, callbacks] : message_callbacks_)
        {
            if (pattern == topic)
            {
                continue;
            }
            if (pattern.find('+') == std::string::npos && pattern.find('#') == std::string::npos)
            {
                continue;
            }
            if (topic_matches(topic, pattern))
            {
                for (const auto& callback : callbacks)
                {
                    matching_callbacks.push_back(callback.callback);
                }
            }
        }
    }

    for (const auto& callback : matching_callbacks)
    {
        callback(topic, parsed_message);
    }
}

void CyberwaveMQTTClient::resubscribe_registered_topics()
{
    std::vector<std::pair<std::string, int>> topics;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        topics.reserve(subscription_qos_.size());
        for (const auto& [topic, qos] : subscription_qos_)
        {
            topics.emplace_back(topic, qos);
        }
    }

    for (const auto& [topic, qos] : topics)
    {
        try
        {
            auto token = client_->subscribe(topic, qos);
            if (!token->wait_for(std::chrono::seconds(5)))
            {
                mqtt_log().warn("MQTT resubscribe timed out after 5s: {}", topic);
            }
            else
            {
                mqtt_log().info("Resubscribed to topic: {}", topic);
            }
        }
        catch (const mqtt::exception& exc)
        {
            mqtt_log().error("MQTT resubscribe error for {}: {}", topic, exc.what());
        }
    }
}

bool CyberwaveMQTTClient::topic_matches(const std::string& topic, const std::string& pattern)
{
    if (pattern == topic)
    {
        return true;
    }

    auto topic_levels = split_topic(topic);
    auto pattern_levels = split_topic(pattern);

    size_t t = 0;
    size_t p = 0;
    while (t < topic_levels.size() && p < pattern_levels.size())
    {
        if (pattern_levels[p] == "#")
        {
            return true;
        }
        if (pattern_levels[p] == "+" || pattern_levels[p] == topic_levels[t])
        {
            ++t;
            ++p;
            continue;
        }
        return false;
    }
    return t == topic_levels.size() && p == pattern_levels.size();
}

std::vector<std::string> CyberwaveMQTTClient::split_topic(const std::string& topic)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start < topic.size())
    {
        const std::size_t slash = topic.find('/', start);
        const std::size_t end = (slash == std::string::npos) ? topic.size() : slash;
        if (end > start)
            parts.emplace_back(topic.substr(start, end - start));
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return parts;
}

void CyberwaveMQTTClient::MQTTCallback::message_arrived(mqtt::const_message_ptr msg)
{
    client_.handle_message(msg->get_topic(), msg->to_string());
}

void CyberwaveMQTTClient::MQTTCallback::connected(const std::string& cause)
{
    client_.reconnect_attempts_ = 0;
    client_.connected_ = true;
    if (cause.empty())
    {
        mqtt_log().info("MQTT connection established");
    }
    else
    {
        mqtt_log().info("MQTT connection established (cause: {})", cause);
    }
    client_.resubscribe_registered_topics();
}

void CyberwaveMQTTClient::MQTTCallback::connection_lost(const std::string& cause)
{
    if (cause.empty())
    {
        mqtt_log().warn("MQTT connection lost");
    }
    else
    {
        mqtt_log().warn("MQTT connection lost: {}", cause);
    }
    client_.connected_ = false;
    client_.reconnect_attempts_ += 1;
    if (client_.reconnect_attempts_ < client_.max_reconnect_attempts_)
    {
        mqtt_log().info("Attempting to reconnect ({}/{})...", client_.reconnect_attempts_,
                        client_.max_reconnect_attempts_);
    }
    else
    {
        mqtt_log().error("Max reconnection attempts reached");
    }
}

void CyberwaveMQTTClient::MQTTCallback::delivery_complete(mqtt::delivery_token_ptr token) { (void)token; }

} // namespace cyberwave