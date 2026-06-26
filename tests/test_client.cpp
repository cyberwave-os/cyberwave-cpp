/**
 * Symmetric with Python: Client from config, disconnect, affect.
 */
#include "../src/cyberwave/client_internal.h"

#include <atomic>
#include <cassert>
#include <cpprest/http_listener.h>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/joints.h>
#include <cyberwave/mqtt_interface.h>
#include <cyberwave/twin.h>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cyberwave;
namespace http_listener = web::http::experimental::listener;

namespace
{

std::string to_std(const utility::string_t& value)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return value;
#else
    return utility::conversions::to_utf8string(value);
#endif
}

utility::string_t to_utility(const std::string& value) { return utility::conversions::to_string_t(value); }

class TestHttpServer
{
public:
    using Handler = std::function<void(web::http::http_request)>;

    explicit TestHttpServer(Handler handler)
        : base_url_("http://127.0.0.1:" + std::to_string(next_port_++)), listener_(to_utility(base_url_)),
          handler_(std::move(handler))
    {
        listener_.support([this](web::http::http_request request) { handler_(std::move(request)); });
        listener_.open().wait();
    }

    ~TestHttpServer() { listener_.close().wait(); }

    const std::string& base_url() const noexcept { return base_url_; }

private:
    static std::atomic<int> next_port_;

    std::string base_url_;
    http_listener::http_listener listener_;
    Handler handler_;
};

std::atomic<int> TestHttpServer::next_port_{32260};

} // namespace

struct MockMqttClient : IMqttClient
{
    bool disconnect_called = false;
    bool connected = true;
    std::string last_topic;
    std::string last_payload;
    std::string last_joint_twin_uuid;
    std::string last_joint_name;
    double last_joint_position = 0.0;
    double last_joint_timestamp = -1.0;
    std::string last_joint_source_type;

    bool is_connected() const override { return connected; }
    std::string get_topic_prefix() const override { return ""; }
    void update_joint_state(const std::string&, const std::string&, double) override {}
    void update_joint_state(const std::string& twin_uuid, const std::string& joint_name, double position_rad,
                            std::optional<double>, std::optional<double>, double timestamp,
                            const std::string& source_type) override
    {
        last_joint_twin_uuid = twin_uuid;
        last_joint_name = joint_name;
        last_joint_position = position_rad;
        last_joint_timestamp = timestamp;
        last_joint_source_type = source_type;
    }
    void publish(const std::string& topic, const std::string& payload) override
    {
        last_topic = topic;
        last_payload = payload;
    }
    void subscribe(const std::string&, MqttMessageHandler) override {}
    void disconnect() override
    {
        disconnect_called = true;
        connected = false;
    }
};

static void test_construct_from_config()
{
    Config cfg;
    cfg.base_url = "https://api.example.com";
    cfg.api_key = "key";
    Client c(cfg);
    assert(c.config().base_url == "https://api.example.com");
    assert(c.config().api_key == "key");
}

static void test_rest_config_injects_bearer_header()
{
    Config cfg;
    cfg.base_url = "https://api.example.com";
    cfg.api_key = "key";
    Client c(cfg);

    auto* config = ClientAccess::api_config(&c);
    assert(config != nullptr);

    const auto& headers = config->getDefaultHeaders();
    const auto header_it = headers.find(utility::conversions::to_string_t("Authorization"));
    assert(header_it != headers.end());
    assert(header_it->second == utility::conversions::to_string_t("Bearer key"));
}

static void test_twin_stub()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("my-twin-uuid");
    assert(t.uuid() == "my-twin-uuid");
    assert(t.name() == "my-twin-uuid");
    assert(&t.client() == &c);
}

static void test_twin_fetches_existing_twin_from_rest()
{
    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            if (path == "/api/v1/twins/existing-twin")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"existing-twin\",\"name\":\"Fetched Twin\","
                                         "\"environment_uuid\":\"env-123\"}"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "key";
    Client c(cfg);

    Twin t = c.twin("existing-twin");
    assert(t.uuid() == "existing-twin");
    assert(t.name() == "Fetched Twin");
    assert(t.environment_id() == "env-123");
}

static void test_twin_from_rest_supports_capability_helpers()
{
    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            if (path == "/api/v1/twins/loco-twin")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"loco-twin\",\"name\":\"Locomote Twin\","
                                         "\"capabilities\":{\"can_locomote\":true}}"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "key";
    Client c(cfg);
    auto mqtt = std::make_shared<MockMqttClient>();
    c.set_mqtt_client(mqtt);

    Twin t = c.twin("loco-twin");
    assert(t.can_locomote());
    t.move_forward(1.25);

    const auto payload = nlohmann::json::parse(mqtt->last_payload);
    assert(payload.at("source_type") == "tele");
    assert(payload.at("command") == "move_forward");
    assert(payload.at("data").at("linear_x") == 1.25);
}

static void test_twin_dispatch_velocity_uses_backend_control()
{
    std::vector<nlohmann::json> dispatch_bodies;

    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            if (path == "/api/v1/twins/loco-twin")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"loco-twin\",\"name\":\"Locomote Twin\","
                                         "\"environment_uuid\":\"env-123\","
                                         "\"capabilities\":{\"can_locomote\":true}}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/agents/environments/env-123/control/actions/dispatch")
            {
                dispatch_bodies.push_back(nlohmann::json::parse(to_std(request.extract_string().get())));
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"action_id\":\"action-1\",\"status\":\"queued\","
                                         "\"message\":\"queued\"}"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "key";
    Client c(cfg);

    Twin t = c.twin("loco-twin");
    const auto response = nlohmann::json::parse(t.dispatch_velocity(
        build_locomotion_velocity_command(0.4, 0.0, 0.1, 750, "trot", "teleop"), "simulation", "mujoco"));

    assert(response.at("action_id") == "action-1");
    const auto dispatch_body = dispatch_bodies.at(0);
    assert(dispatch_body.at("mode") == "simulation");
    assert(dispatch_body.at("confirmed") == false);
    assert(dispatch_body.at("simulation_backend") == "mujoco");
    const auto action = dispatch_body.at("action");
    assert(action.at("kind") == "controller_policy_execute");
    assert(action.at("target_twin_uuid") == "loco-twin");
    const auto payload = action.at("payload");
    assert(payload.at("runtime_kind") == "simulation");
    assert(payload.at("simulation_backend") == "mujoco");
    assert(payload.at("velocity_command").at("contract") == LOCOMOTION_VELOCITY_COMMAND_CONTRACT);
    assert(payload.at("velocity_command").at("linear_x") == 0.4);
    assert(payload.at("velocity_command").at("angular_z") == 0.1);

    t.drive_forward(0.25, 1000, "simulation", "mujoco");
    const auto drive_payload = dispatch_bodies.at(1).at("action").at("payload");
    assert(drive_payload.at("velocity_command").at("linear_x") == 0.25);
    assert(drive_payload.at("velocity_command").at("duration_ms") == 1000);

    t.turn_velocity_right(0.6, 1000, "simulation", "mujoco");
    const auto turn_payload = dispatch_bodies.at(2).at("action").at("payload");
    assert(turn_payload.at("velocity_command").at("angular_z") == -0.6);

    t.stop_velocity("simulation", "mujoco");
    const auto stop_payload = dispatch_bodies.at(3).at("action").at("payload");
    assert(stop_payload.at("velocity_command").at("linear_x") == 0.0);
    assert(stop_payload.at("velocity_command").at("angular_z") == 0.0);
    assert(stop_payload.at("velocity_command").at("duration_ms") == 0);
    assert(stop_payload.at("velocity_command").at("gait") == "stand");

    t.dispatch_velocity(build_locomotion_velocity_command(0.2, 0.0, 0.0, 500, "walk", "teleop"),
                        PolicyRefPayload{"catalog_seed_id", "controller:go2-mujoco-velocity-policy:v1"}, "simulation",
                        "mujoco");
    const auto policy_ref_action = dispatch_bodies.at(4).at("action");
    assert(policy_ref_action.at("policy_ref").at("kind") == "catalog_seed_id");
    assert(policy_ref_action.at("policy_ref").at("value") == "controller:go2-mujoco-velocity-policy:v1");
    assert(policy_ref_action.at("payload").at("policy_ref").at("kind") == "catalog_seed_id");
}

static void test_twin_resolves_asset_key_and_reuses_existing_twin()
{
    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            if (path == "/api/v1/twins/camera")
            {
                request.reply(web::http::status_codes::NotFound, to_utility("not found"));
                return;
            }
            if (path == "/api/v1/assets/camera")
            {
                request.reply(web::http::status_codes::NotFound, to_utility("asset not found"));
                return;
            }
            if (path == "/api/v1/assets")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("[{\"uuid\":\"asset-uuid\",\"name\":\"Camera\","
                                         "\"registry_id\":\"cyberwave/camera\",\"registry_id_alias\":\"camera\"}]"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/assets/asset-uuid")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"asset-uuid\",\"name\":\"Camera\","
                                         "\"registry_id\":\"cyberwave/camera\",\"registry_id_alias\":\"camera\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/environments/env-123/twins")
            {
                request.reply(
                    web::http::status_codes::OK,
                    to_utility("[{\"uuid\":\"twin-uuid\",\"name\":\"Camera Twin\",\"asset_uuid\":\"asset-uuid\","
                               "\"environment_uuid\":\"env-123\"}]"),
                    to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "key";
    cfg.environment_id = "env-123";
    Client c(cfg);

    Twin t = c.twin("camera");
    assert(t.uuid() == "twin-uuid");
    assert(t.name() == "Camera Twin");
    assert(t.environment_id() == "env-123");
}

static void test_twin_creates_missing_asset_twin()
{
    std::mutex mutex;
    std::vector<std::string> request_bodies;

    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            if (path == "/api/v1/twins/camera")
            {
                request.reply(web::http::status_codes::NotFound, to_utility("not found"));
                return;
            }
            if (path == "/api/v1/assets/camera")
            {
                request.reply(web::http::status_codes::NotFound, to_utility("asset not found"));
                return;
            }
            if (path == "/api/v1/assets")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("[{\"uuid\":\"asset-uuid\",\"name\":\"Camera\","
                                         "\"registry_id\":\"cyberwave/camera\",\"registry_id_alias\":\"camera\","
                                         "\"fixed_base\":true}]"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/assets/asset-uuid")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"asset-uuid\",\"name\":\"Camera\","
                                         "\"registry_id\":\"cyberwave/camera\",\"registry_id_alias\":\"camera\","
                                         "\"fixed_base\":true}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/environments/env-123/twins")
            {
                request.reply(web::http::status_codes::OK, to_utility("[]"), to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/twins" && request.method() == web::http::methods::POST)
            {
                const std::string body = to_std(request.extract_string().get());
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    request_bodies.push_back(body);
                }
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"created-twin\",\"name\":\"Created Camera\","
                                         "\"asset_uuid\":\"asset-uuid\",\"environment_uuid\":\"env-123\","
                                         "\"fixed_base\":true}"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "key";
    Client c(cfg);

    TwinResolveOptions options;
    options.environment_id = "env-123";
    options.name = "Created Camera";

    Twin t = c.twin("camera", options);
    assert(t.uuid() == "created-twin");
    assert(t.name() == "Created Camera");
    assert(t.environment_id() == "env-123");

    std::lock_guard<std::mutex> lock(mutex);
    assert(request_bodies.size() == 1);
    const auto payload = nlohmann::json::parse(request_bodies.front());
    assert(payload.at("asset_uuid") == "asset-uuid");
    assert(payload.at("environment_uuid") == "env-123");
    assert(payload.at("name") == "Created Camera");
    assert(payload.at("fixed_base") == true);
}

static void test_twin_bootstraps_quickstart_context_when_environment_missing()
{
    std::mutex mutex;
    std::vector<std::string> request_bodies;

    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            if (path == "/api/v1/twins/camera")
            {
                request.reply(web::http::status_codes::NotFound, to_utility("not found"));
                return;
            }
            if (path == "/api/v1/users/workspaces" && request.method() == web::http::methods::GET)
            {
                request.reply(web::http::status_codes::OK, to_utility("[]"), to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/users/workspaces" && request.method() == web::http::methods::POST)
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"ws-123\",\"name\":\"Quickstart Workspace\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/projects" && request.method() == web::http::methods::GET)
            {
                request.reply(web::http::status_codes::OK, to_utility("[]"), to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/projects" && request.method() == web::http::methods::POST)
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"proj-123\",\"name\":\"Quickstart Project\","
                                         "\"workspace_uuid\":\"ws-123\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/projects/proj-123/environments" && request.method() == web::http::methods::POST)
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"env-123\",\"name\":\"Quickstart Environment\","
                                         "\"project_uuid\":\"proj-123\",\"workspace_uuid\":\"ws-123\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/assets/camera")
            {
                request.reply(web::http::status_codes::NotFound, to_utility("asset not found"));
                return;
            }
            if (path == "/api/v1/assets")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("[{\"uuid\":\"asset-uuid\",\"name\":\"Camera\","
                                         "\"registry_id\":\"cyberwave/camera\",\"registry_id_alias\":\"camera\","
                                         "\"fixed_base\":true}]"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/assets/asset-uuid")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"asset-uuid\",\"name\":\"Camera\","
                                         "\"registry_id\":\"cyberwave/camera\",\"registry_id_alias\":\"camera\","
                                         "\"fixed_base\":true}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/environments/env-123/twins")
            {
                request.reply(web::http::status_codes::OK, to_utility("[]"), to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/twins" && request.method() == web::http::methods::POST)
            {
                const std::string body = to_std(request.extract_string().get());
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    request_bodies.push_back(body);
                }
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"quickstart-twin\",\"name\":\"Camera\","
                                         "\"asset_uuid\":\"asset-uuid\",\"environment_uuid\":\"env-123\"}"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "key";
    Client c(cfg);

    Twin t = c.twin("camera");
    assert(t.uuid() == "quickstart-twin");
    assert(t.environment_id() == "env-123");
    assert(c.config().workspace_id == "ws-123");
    assert(c.config().environment_id == "env-123");

    std::lock_guard<std::mutex> lock(mutex);
    assert(request_bodies.size() == 1);
    const auto payload = nlohmann::json::parse(request_bodies.front());
    assert(payload.at("asset_uuid") == "asset-uuid");
    assert(payload.at("environment_uuid") == "env-123");
}

static void test_disconnect()
{
    TestHttpServer server(
        [](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            if (path == "/api/v1/twins/x")
            {
                request.reply(web::http::status_codes::OK, to_utility("{\"uuid\":\"x\",\"name\":\"x\"}"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "key";
    Client c(cfg);
    auto mqtt = std::make_shared<MockMqttClient>();
    c.set_mqtt_client(mqtt);
    auto* api_before = ClientAccess::default_api(&c);
    assert(api_before != nullptr);
    c.disconnect();
    assert(mqtt->disconnect_called);
    assert(!c.mqtt_client());
    assert(ClientAccess::default_api(&c) == api_before);
    Twin t = c.twin("x");
    assert(t.uuid() == "x");
}

static void test_affect_simulation()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    c.affect("simulation");
    assert(c.source_type() == "sim");
    assert(c.config().runtime_mode == "simulation");
    c.affect("sim");
    assert(c.source_type() == "sim");
}

static void test_affect_real_world()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    c.affect("real-world");
    assert(c.source_type() == "edge");
    assert(c.config().runtime_mode == "live");
    c.affect("real");
    assert(c.source_type() == "edge");
    c.affect("tele");
    assert(c.source_type() == "edge");
    c.affect("teleoperation");
    assert(c.source_type() == "edge");
    c.affect("live");
    assert(c.source_type() == "edge");
}

static void test_affect_chain()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    c.affect("sim").affect("real-world");
    assert(c.source_type() == "edge");
}

static void test_affect_disconnects_attached_mqtt_client()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto mqtt = std::make_shared<MockMqttClient>();
    c.set_mqtt_client(mqtt);

    c.affect("simulation");
    assert(mqtt->disconnect_called);
    assert(!c.mqtt_client());
}

static void test_affect_unknown_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    bool threw = false;
    try
    {
        c.affect("bogus");
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_source_type_default()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    assert(c.source_type() == "edge");
}

static void test_publish_event_uses_mqtt_topic_and_payload()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto mqtt = std::make_shared<MockMqttClient>();
    c.set_mqtt_client(mqtt);
    c.publish_event("twin-uuid", "person_\"detected", "{\"count\":3,\"tag\":\"alpha\\nbeta\"}", "edge\"node");
    assert(mqtt->last_topic == "cyberwave/twin/twin-uuid/event");
    const auto payload = nlohmann::json::parse(mqtt->last_payload);
    assert(payload.at("event_type") == "person_\"detected");
    assert(payload.at("source") == "edge\"node");
    assert(payload.at("data").at("count") == 3);
    assert(payload.at("data").at("tag") == "alpha\nbeta");
    assert(payload.contains("timestamp"));
    assert(payload.at("timestamp").is_number());
}

static void test_publish_event_rejects_invalid_json_payload()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto mqtt = std::make_shared<MockMqttClient>();
    c.set_mqtt_client(mqtt);

    bool threw = false;
    try
    {
        c.publish_event("twin-uuid", "person_detected", "{invalid");
    }
    catch (const CyberwaveValidationError&)
    {
        threw = true;
    }

    assert(threw);
}

static void test_joint_controller_uses_runtime_mode_control_source_type()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto mqtt = std::make_shared<MockMqttClient>();
    c.set_mqtt_client(mqtt);

    Twin t(c, "joint-twin", "Joint Twin");
    t.joints().set("shoulder_joint", 90.0);
    assert(mqtt->last_joint_twin_uuid == "joint-twin");
    assert(mqtt->last_joint_name == "shoulder_joint");
    assert(mqtt->last_joint_source_type == "tele");

    c.affect("simulation");
    auto mqtt_sim = std::make_shared<MockMqttClient>();
    c.set_mqtt_client(mqtt_sim);
    t.joints().set("shoulder_joint", 45.0);
    assert(mqtt_sim->last_joint_source_type == "sim_tele");
}

static void test_twin_get_latest_frame_follows_affect_source_type()
{
    std::mutex mutex;
    std::vector<std::string> request_queries;

    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            const std::string query = to_std(request.relative_uri().query());
            if (path == "/api/v1/twins/frame-twin/latest-frame")
            {
                std::lock_guard<std::mutex> lock(mutex);
                request_queries.push_back(query);
                request.reply(web::http::status_codes::OK, to_utility("JPEGDATA"), to_utility("image/jpeg"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "key";
    Client c(cfg);
    Twin t(c, "frame-twin", "Frame Twin");

    const auto live_frame = t.get_latest_frame();
    c.affect("simulation");
    const auto sim_frame = t.get_latest_frame();

    assert(std::string(live_frame.begin(), live_frame.end()) == "JPEGDATA");
    assert(std::string(sim_frame.begin(), sim_frame.end()) == "JPEGDATA");

    std::lock_guard<std::mutex> lock(mutex);
    assert(request_queries.size() == 2);
    assert(request_queries[0].empty());
    assert(request_queries[1] == "source_type=sim");
}

int main()
{
    test_construct_from_config();
    test_rest_config_injects_bearer_header();
    test_twin_stub();
    test_twin_fetches_existing_twin_from_rest();
    test_twin_from_rest_supports_capability_helpers();
    test_twin_dispatch_velocity_uses_backend_control();
    test_twin_resolves_asset_key_and_reuses_existing_twin();
    test_twin_creates_missing_asset_twin();
    test_twin_bootstraps_quickstart_context_when_environment_missing();
    test_disconnect();
    test_affect_simulation();
    test_affect_real_world();
    test_affect_chain();
    test_affect_disconnects_attached_mqtt_client();
    test_affect_unknown_throws();
    test_source_type_default();
    test_publish_event_uses_mqtt_topic_and_payload();
    test_publish_event_rejects_invalid_json_payload();
    test_joint_controller_uses_runtime_mode_control_source_type();
    test_twin_get_latest_frame_follows_affect_source_type();
    return 0;
}
