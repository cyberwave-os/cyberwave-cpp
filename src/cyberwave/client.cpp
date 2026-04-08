#include "cyberwave/client.h"
#include "cyberwave/alerts.h"
#include "cyberwave/assets.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/data.h"
#include "cyberwave/edges.h"
#include "cyberwave/environments.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/projects.h"
#include "cyberwave/scene.h"
#include "cyberwave/twin.h"
#include "cyberwave/twins.h"
#include "cyberwave/workers.h"
#include "cyberwave/workflows.h"
#include "cyberwave/workspaces.h"

#include "CppRestOpenAPIClient/ApiClient.h"
#include "CppRestOpenAPIClient/ApiConfiguration.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cpprest/details/basic_types.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

namespace cyberwave
{

namespace
{

bool is_not_found_error(const CyberwaveAPIError& error)
{
    if (error.status_code() == 404)
    {
        return true;
    }
    if (error.status_code() != 0)
    {
        return false;
    }

    std::string message = error.what();
    std::transform(message.begin(), message.end(), message.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    return message.find("404") != std::string::npos || message.find("not found") != std::string::npos ||
           message.find("does not exist") != std::string::npos;
}

std::string ensure_quickstart_environment(const Client& client, Config& config)
{
    if (!config.environment_id.empty())
        return config.environment_id;

    std::string workspace_id = config.workspace_id;
    if (workspace_id.empty())
    {
        const auto workspaces = client.workspaces().list();
        if (workspaces.empty())
        {
            workspace_id = client.workspaces().create("Quickstart Workspace").uuid();
        }
        else
        {
            workspace_id = workspaces.front().uuid();
        }
        config.workspace_id = workspace_id;
    }

    const auto projects = client.projects().list(workspace_id);
    std::string project_id;
    if (projects.empty())
    {
        project_id = client.projects().create("Quickstart Project", workspace_id).uuid();
    }
    else
    {
        project_id = projects.front().uuid();
    }

    config.environment_id = client.environments().create("Quickstart Environment", project_id).uuid();
    return config.environment_id;
}

} // namespace

struct Client::RestState
{
    std::shared_ptr<org::openapitools::client::api::ApiConfiguration> config;
    std::shared_ptr<org::openapitools::client::api::ApiClient> api_client;
    std::shared_ptr<org::openapitools::client::api::DefaultApi> default_api;
};

Client::Client(const Config& config) : config_(config), hook_registry_(std::make_unique<HookRegistry>())
{
    if (config_.api_key.empty())
        return;
    rest_ = std::make_unique<RestState>();
    rest_->config = std::make_shared<org::openapitools::client::api::ApiConfiguration>();
    rest_->config->setBaseUrl(utility::conversions::to_string_t(config_.base_url));
    rest_->config->getDefaultHeaders()[utility::conversions::to_string_t("Authorization")] =
        utility::conversions::to_string_t("Bearer " + config_.api_key);
    rest_->config->setApiKey(utility::conversions::to_string_t("CustomTokenAuthentication"),
                             utility::conversions::to_string_t("Bearer " + config_.api_key));
    rest_->api_client = std::make_shared<org::openapitools::client::api::ApiClient>(rest_->config);
    rest_->default_api = std::make_shared<org::openapitools::client::api::DefaultApi>(rest_->api_client);
}

Client::Client(Config&& config) : config_(std::move(config)), hook_registry_(std::make_unique<HookRegistry>())
{
    if (config_.api_key.empty())
        return;
    rest_ = std::make_unique<RestState>();
    rest_->config = std::make_shared<org::openapitools::client::api::ApiConfiguration>();
    rest_->config->setBaseUrl(utility::conversions::to_string_t(config_.base_url));
    rest_->config->getDefaultHeaders()[utility::conversions::to_string_t("Authorization")] =
        utility::conversions::to_string_t("Bearer " + config_.api_key);
    rest_->config->setApiKey(utility::conversions::to_string_t("CustomTokenAuthentication"),
                             utility::conversions::to_string_t("Bearer " + config_.api_key));
    rest_->api_client = std::make_shared<org::openapitools::client::api::ApiClient>(rest_->config);
    rest_->default_api = std::make_shared<org::openapitools::client::api::DefaultApi>(rest_->api_client);
}

Client::~Client() { lifetime_token_.reset(); }

org::openapitools::client::api::DefaultApi* ClientAccess::default_api(const Client* client)
{
    return client && client->rest_ && client->rest_->default_api ? client->rest_->default_api.get() : nullptr;
}

org::openapitools::client::api::ApiConfiguration* ClientAccess::api_config(const Client* client)
{
    return client && client->rest_ && client->rest_->config ? client->rest_->config.get() : nullptr;
}

std::weak_ptr<void> ClientAccess::lifetime_token(const Client* client)
{
    return client ? std::weak_ptr<void>(client->lifetime_token_) : std::weak_ptr<void>{};
}

Twin Client::twin(const std::string& identifier, const TwinResolveOptions& options) const
{
    const std::string requested_twin_id = options.twin_id.empty() ? identifier : options.twin_id;
    if (!rest_)
    {
        const std::string stub_id = requested_twin_id.empty() ? identifier : requested_twin_id;
        const std::string stub_name = options.name.empty() ? stub_id : options.name;
        Twin stub(*this, stub_id, stub_name);
        if (!options.environment_id.empty())
        {
            stub.set_environment_id(options.environment_id);
        }
        else if (!config_.environment_id.empty())
        {
            stub.set_environment_id(config_.environment_id);
        }
        return stub;
    }

    if (!requested_twin_id.empty())
    {
        try
        {
            return twins().get(requested_twin_id);
        }
        catch (const CyberwaveAPIError& error)
        {
            if (!options.twin_id.empty() || !is_not_found_error(error))
            {
                throw;
            }
        }
    }

    if (identifier.empty())
    {
        throw CyberwaveValidationError(
            "Client::twin() requires an asset registry id or alias when twin_id is not provided");
    }

    std::string environment_id = options.environment_id.empty() ? config_.environment_id : options.environment_id;
    if (environment_id.empty())
        environment_id = ensure_quickstart_environment(*this, config_);

    const Asset asset = assets().get_by_registry_id(identifier);
    if (options.reuse_existing)
    {
        for (const Twin& existing : twins().list(environment_id))
        {
            if (existing.asset_id() == asset.uuid() && (options.name.empty() || existing.name() == options.name))
            {
                return existing;
            }
        }
    }

    if (!options.create_if_missing)
    {
        throw CyberwaveError("No matching twin found for asset '" + identifier + "' in environment '" + environment_id +
                             "'");
    }

    return twins().create(asset.uuid(), environment_id, options.name, options.description, options.position,
                          options.orientation, options.fixed_base.value_or(asset.fixed_base()));
}

WorkspaceManager Client::workspaces() const { return WorkspaceManager(*this); }

EdgeManager Client::edges() const { return EdgeManager(*this); }

ProjectManager Client::projects() const { return ProjectManager(*this); }

EnvironmentManager Client::environments() const { return EnvironmentManager(*this); }

AssetManager Client::assets() const { return AssetManager(*this); }

TwinManager Client::twins() const { return TwinManager(*this); }

WorkflowManager Client::workflows() const { return WorkflowManager(*this); }

WorkflowRunManager Client::workflow_runs() const { return WorkflowRunManager(*this); }

DataBus Client::data(std::shared_ptr<DataBackend> backend, const std::string& sensor_name,
                     const std::string& key_prefix) const
{
    if (config_.twin_uuid.empty())
        throw CyberwaveError("Client::data() requires Config.twin_uuid or CYBERWAVE_TWIN_UUID");
    return DataBus(std::move(backend), config_.twin_uuid,
                   sensor_name.empty() ? std::nullopt : std::optional<std::string>(sensor_name), key_prefix);
}

HookRegistry& Client::hooks() { return *hook_registry_; }

const HookRegistry& Client::hooks() const { return *hook_registry_; }

Scene Client::get_scene(const std::string& environment_id) const { return Scene(*this, environment_id); }

void Client::disconnect()
{
    if (mqtt_)
    {
        mqtt_->disconnect();
        mqtt_.reset();
    }
}

Client& Client::affect(const std::string& mode)
{
    static const std::map<std::string, std::string> mode_map = {
        {"simulation", "simulation"}, {"sim", "simulation"}, {"sim_tele", "simulation"},
        {"real-world", "live"},       {"real", "live"},      {"tele", "live"},
        {"teleoperation", "live"},    {"live", "live"},
    };
    std::string lower = mode;
    for (auto& ch : lower)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    // trim
    auto start = lower.find_first_not_of(" \t");
    auto end = lower.find_last_not_of(" \t");
    lower = (start == std::string::npos) ? "" : lower.substr(start, end - start + 1);
    auto it = mode_map.find(lower);
    if (it == mode_map.end())
        throw std::invalid_argument("Unknown mode '" + mode + "'. Use 'simulation' or 'real-world'.");
    config_.runtime_mode = it->second;
    config_.source_type = (it->second == "simulation") ? "sim" : "edge";
    disconnect();
    return *this;
}

void Client::set_mqtt_client(std::shared_ptr<IMqttClient> mqtt) { mqtt_ = std::move(mqtt); }

void Client::publish_event(const std::string& twin_uuid, const std::string& event_type, const std::string& data_json,
                           const std::string& source) const
{
    if (!mqtt_)
        throw CyberwaveError("publish_event() requires an MQTT client set on Client (set_mqtt_client)");

    const auto now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    nlohmann::json data = nlohmann::json::object();
    if (!data_json.empty())
    {
        try
        {
            data = nlohmann::json::parse(data_json);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            throw CyberwaveValidationError("publish_event() data_json must be valid JSON: " + std::string(e.what()));
        }
    }

    const nlohmann::json payload = {
        {"event_type", event_type},
        {"source", source},
        {"data", data},
        {"timestamp", now},
    };

    mqtt_->publish(mqtt_->get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/event", payload.dump());
}

} // namespace cyberwave
