#include "mqtt_client.h"
#include "constants.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string_view>

using json = nlohmann::json;

namespace cyberwave
{

// ── MosquittoDeleter ────────────────────────────────────────────────────────

void MosquittoDeleter::operator()(struct mosquitto* m) const noexcept
{
    if (m)
    {
        mosquitto_destroy(m);
    }
}

namespace
{

// ── Library lifecycle (process-global, init once) ───────────────────────────

std::once_flag mosq_lib_init_flag;

void ensure_mosq_lib_init()
{
    std::call_once(mosq_lib_init_flag,
                   []
                   {
                       mosquitto_lib_init();
                       std::atexit([] { mosquitto_lib_cleanup(); });
                   });
}

// ── Logging ─────────────────────────────────────────────────────────────────

inline spdlog::logger& mqtt_log()
{
    if (auto l = spdlog::get("cyberwave.mqtt"); l)
    {
        return *l;
    }
    auto l = spdlog::stdout_color_mt("cyberwave.mqtt");
    l->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
    return *l;
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

// ── Constructor / Destructor ────────────────────────────────────────────────

CyberwaveMQTTClient::CyberwaveMQTTClient(const CyberwaveConfig& config) : config_(config)
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

    if (has_nonempty_env("CYBERWAVE_MQTT_USE_TLS"))
    {
        const std::string env_mqtt_tls = read_env("CYBERWAVE_MQTT_USE_TLS");
        try
        {
            config_.mqtt_use_tls = parse_bool_env(env_mqtt_tls);
        }
        catch (const std::invalid_argument&)
        {
        }
    }

    if (config_.mqtt_tls_ca_cert.empty())
    {
        config_.mqtt_tls_ca_cert = read_env("CYBERWAVE_MQTT_TLS_CA_CERT");
    }

    client_id_ = (config_.runtime_mode == "simulation" ? "sdk_sim_" : "sdk_") + generate_client_id();

    ensure_mosq_lib_init();
    mosq_.reset(mosquitto_new(client_id_.c_str(), true, this));
    if (!mosq_)
    {
        throw std::runtime_error("mosquitto_new failed (out of memory)");
    }

    mosquitto_connect_callback_set(mosq_.get(), on_connect_cb);
    mosquitto_disconnect_callback_set(mosq_.get(), on_disconnect_cb);
    mosquitto_message_callback_set(mosq_.get(), on_message_cb);
}

CyberwaveMQTTClient::~CyberwaveMQTTClient()
{
    disconnect();
    if (loop_started_)
    {
        mosquitto_loop_stop(mosq_.get(), true);
        loop_started_ = false;
    }
}

// ── Connection ──────────────────────────────────────────────────────────────

void CyberwaveMQTTClient::connect()
{
    if (connected_)
    {
        return;
    }

    const bool use_tls = config_.mqtt_use_tls;

    mosquitto_username_pw_set(mosq_.get(), mqtt_username_.c_str(), mqtt_api_token_.c_str());
    mosquitto_reconnect_delay_set(mosq_.get(), 1, 30, true);

    if (config_.mqtt_protocol > 0)
    {
        mosquitto_int_option(mosq_.get(), MOSQ_OPT_PROTOCOL_VERSION, config_.mqtt_protocol);
    }

    if (use_tls)
    {
        std::string ca_path;
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
                ca_path = config_.mqtt_tls_ca_cert;
            }
            else
            {
                mqtt_log().warn("MQTT TLS CA cert path not found; falling back to system trust store: '{}'",
                                config_.mqtt_tls_ca_cert);
            }
        }

        if (ca_path.empty())
        {
            const char* default_ca_paths[] = {
                "/etc/ssl/certs/ca-certificates.crt",
                "/etc/ssl/cert.pem",
            };

            for (const char* path : default_ca_paths)
            {
                if (!path)
                    continue;
                try
                {
                    if (std::filesystem::exists(path))
                    {
                        ca_path = path;
                        mqtt_log().info("MQTT TLS CA cert not configured; using default CA bundle: '{}'", path);
                        break;
                    }
                }
                catch (...)
                {
                }
            }
            if (ca_path.empty())
            {
                mqtt_log().warn(
                    "MQTT TLS CA cert not configured and default CA bundles not found; relying on MQTT TLS defaults");
            }
        }

        int rc = mosquitto_tls_set(mosq_.get(), ca_path.empty() ? nullptr : ca_path.c_str(), nullptr, nullptr, nullptr,
                                   nullptr);
        if (rc != MOSQ_ERR_SUCCESS)
        {
            std::string message = std::string("MQTT TLS setup error: ") + mosquitto_strerror(rc);
            mqtt_log().error("{}", message);
            throw std::runtime_error(message);
        }
        mosquitto_tls_insecure_set(mosq_.get(), false);
    }

    mqtt_log().info("Connecting to MQTT broker: {}:{} ({})", mqtt_broker_, mqtt_port_,
                    use_tls ? "TLS enabled" : "TLS disabled");

    int rc = mosquitto_loop_start(mosq_.get());
    if (rc != MOSQ_ERR_SUCCESS)
    {
        std::string message = std::string("Failed to start MQTT loop: ") + mosquitto_strerror(rc);
        mqtt_log().error("{}", message);
        throw std::runtime_error(message);
    }
    loop_started_ = true;

    rc = mosquitto_connect_async(mosq_.get(), mqtt_broker_.c_str(), mqtt_port_, 60);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        std::string message = std::string("MQTT connection error: ") + mosquitto_strerror(rc);
        if (mqtt_port_ == 1883 && use_tls)
        {
            message += " Hint: port 1883 is usually non-TLS. Set CYBERWAVE_MQTT_USE_TLS=false, "
                       "or use port 8883 for TLS.";
        }
        mqtt_log().error("{}", message);
        throw std::runtime_error(message);
    }

    {
        std::unique_lock<std::mutex> lock(connect_mutex_);
        if (!connect_cv_.wait_for(lock, std::chrono::seconds(10), [this] { return connected_.load(); }))
        {
            throw std::runtime_error("MQTT connect timed out after 10s");
        }
    }

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
    }
    catch (...)
    {
        mqtt_log().warn("Error publishing disconnect messages");
    }

    connected_ = false;
    mosquitto_disconnect(mosq_.get());

    if (loop_started_)
    {
        mosquitto_loop_stop(mosq_.get(), false);
        loop_started_ = false;
    }

    mqtt_log().info("Disconnected from MQTT broker");
}

bool CyberwaveMQTTClient::is_connected() const { return connected_; }

std::string CyberwaveMQTTClient::get_topic_prefix() const { return topic_prefix_; }

// ── libmosquitto callbacks ──────────────────────────────────────────────────

void CyberwaveMQTTClient::on_connect_cb(struct mosquitto* /*mosq*/, void* userdata, int rc)
{
    auto& self = *static_cast<CyberwaveMQTTClient*>(userdata);
    if (rc == 0)
    {
        self.reconnect_attempts_ = 0;
        {
            std::lock_guard<std::mutex> lock(self.connect_mutex_);
            self.connected_ = true;
        }
        self.connect_cv_.notify_all();
        mqtt_log().info("MQTT connection established");
        self.resubscribe_registered_topics();
    }
    else
    {
        mqtt_log().error("MQTT connection refused: {}", mosquitto_connack_string(rc));
    }
}

void CyberwaveMQTTClient::on_disconnect_cb(struct mosquitto* /*mosq*/, void* userdata, int rc)
{
    auto& self = *static_cast<CyberwaveMQTTClient*>(userdata);
    self.connected_ = false;

    if (rc == 0)
    {
        mqtt_log().info("MQTT disconnected cleanly");
        return;
    }

    self.reconnect_attempts_ += 1;
    if (self.reconnect_attempts_ < self.max_reconnect_attempts_)
    {
        mqtt_log().warn("MQTT connection lost (rc={}); reconnecting ({}/{})...", rc, self.reconnect_attempts_,
                        self.max_reconnect_attempts_);
    }
    else
    {
        mqtt_log().error("MQTT connection lost (rc={}); max reconnection attempts reached", rc);
    }
}

void CyberwaveMQTTClient::on_message_cb(struct mosquitto* /*mosq*/, void* userdata, const struct mosquitto_message* msg)
{
    if (!msg || !msg->topic)
        return;
    auto& self = *static_cast<CyberwaveMQTTClient*>(userdata);
    std::string topic(msg->topic);
    std::string payload;
    if (msg->payload && msg->payloadlen > 0)
    {
        payload.assign(static_cast<const char*>(msg->payload), static_cast<std::size_t>(msg->payloadlen));
    }
    self.handle_message(topic, payload);
}

// ── Publish ─────────────────────────────────────────────────────────────────

void CyberwaveMQTTClient::publish(const std::string& topic, const json& message, int qos)
{
    if (!connected_)
    {
        mqtt_log().warn("Cannot publish to {}: not connected to MQTT broker", topic);
        return;
    }

    json payload_json = message;
    if (payload_json.is_object() && !payload_json.contains("session_id"))
    {
        payload_json["session_id"] = client_id_;
    }
    std::string payload = payload_json.dump();

    int rc = mosquitto_publish(mosq_.get(), nullptr, topic.c_str(), static_cast<int>(payload.size()), payload.c_str(),
                               qos, false);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        mqtt_log().warn("MQTT publish error on {}: {}", topic, mosquitto_strerror(rc));
    }
}

void CyberwaveMQTTClient::publish(const std::string& topic, const std::string& message, int qos)
{
    if (!connected_)
    {
        mqtt_log().warn("Cannot publish to {}: not connected to MQTT broker", topic);
        return;
    }

    int rc = mosquitto_publish(mosq_.get(), nullptr, topic.c_str(), static_cast<int>(message.size()), message.c_str(),
                               qos, false);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        mqtt_log().warn("MQTT publish error on {}: {}", topic, mosquitto_strerror(rc));
    }
}

// ── Subscribe / Unsubscribe ─────────────────────────────────────────────────

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

    int rc = mosquitto_subscribe(mosq_.get(), nullptr, topic.c_str(), qos);
    if (rc == MOSQ_ERR_SUCCESS)
    {
        mqtt_log().info("Subscribed to topic: {}", topic);
    }
    else
    {
        mqtt_log().error("MQTT subscribe error on {}: {}", topic, mosquitto_strerror(rc));
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
        int rc = mosquitto_unsubscribe(mosq_.get(), nullptr, topic.c_str());
        if (rc != MOSQ_ERR_SUCCESS)
        {
            mqtt_log().error("MQTT unsubscribe error on {}: {}", topic, mosquitto_strerror(rc));
        }
    }
}

// ── Resubscribe (after reconnect) ───────────────────────────────────────────

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
        int rc = mosquitto_subscribe(mosq_.get(), nullptr, topic.c_str(), qos);
        if (rc == MOSQ_ERR_SUCCESS)
        {
            mqtt_log().info("Resubscribed to topic: {}", topic);
        }
        else
        {
            mqtt_log().error("MQTT resubscribe error for {}: {}", topic, mosquitto_strerror(rc));
        }
    }
}

// ── Helpers (unchanged business logic) ──────────────────────────────────────

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

// ── Telemetry lifecycle ─────────────────────────────────────────────────────

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
        if (metadata.contains("camera_participants"))
        {
            message["camera_participants"] = metadata.at("camera_participants");
        }
    }
    publish(with_prefix("cyberwave/twin/" + twin_uuid + "/telemetry"), message);
}

void CyberwaveMQTTClient::publish_telemetry_start(const std::string& twin_uuid, const json& metadata)
{
    handle_twin_update_with_telemetry(twin_uuid, metadata);
}

void CyberwaveMQTTClient::publish_telemetry_end(const std::string& twin_uuid, const json& metadata)
{
    json payload = metadata.is_object() ? metadata : json::object();
    payload["type"] = "telemetry_end";
    payload["timestamp"] = now_seconds();

    publish(with_prefix("cyberwave/twin/" + twin_uuid + "/telemetry"), payload);

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

// ── Twin methods ────────────────────────────────────────────────────────────

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

void CyberwaveMQTTClient::update_twin_gps(const std::string& twin_uuid, const GpsFix& fix)
{
    if (fix.fix_type && *fix.fix_type == "none")
        return;

    const double now = now_seconds();
    const auto last_it = last_gps_update_times_.find(twin_uuid);
    if (last_it != last_gps_update_times_.end() && (now - last_it->second) < kGpsMinUpdateIntervalSec)
        return;
    last_gps_update_times_[twin_uuid] = now;

    const std::string topic = with_prefix("cyberwave/twin/" + twin_uuid + "/gps");
    json message = {
        {"source_type", source_type_}, {"latitude", fix.latitude},   {"longitude", fix.longitude},
        {"altitude", fix.altitude},    {"timestamp", now_seconds()},
    };
    if (fix.satellite_count)
        message["satellite_count"] = *fix.satellite_count;
    if (fix.signal_level)
        message["signal_level"] = *fix.signal_level;
    if (fix.compass_heading)
        message["compass_heading"] = *fix.compass_heading;
    if (fix.horizontal_accuracy)
        message["horizontal_accuracy"] = *fix.horizontal_accuracy;
    if (fix.vertical_accuracy)
        message["vertical_accuracy"] = *fix.vertical_accuracy;
    if (fix.fix_type)
        message["fix_type"] = *fix.fix_type;
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

// ── Environment methods ─────────────────────────────────────────────────────

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

// ── Streaming methods ───────────────────────────────────────────────────────

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

// ── WebRTC methods ──────────────────────────────────────────────────────────

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

// ── Command methods ─────────────────────────────────────────────────────────

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

// ── Ping / pong ─────────────────────────────────────────────────────────────

void CyberwaveMQTTClient::ping(const std::string& resource_uuid)
{
    publish(with_prefix("cyberwave/ping/" + resource_uuid + "/request"),
            json{{"type", "ping"}, {"timestamp", now_seconds()}});
}

void CyberwaveMQTTClient::subscribe_pong(const std::string& resource_uuid, MessageCallback callback)
{
    subscribe(with_prefix("cyberwave/pong/" + resource_uuid + "/response"), callback);
}

// ── Message dispatch ────────────────────────────────────────────────────────

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

// ── Topic matching ──────────────────────────────────────────────────────────

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

} // namespace cyberwave
