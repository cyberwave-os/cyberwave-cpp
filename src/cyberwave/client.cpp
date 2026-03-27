#include "cyberwave/client.h"
#include "cyberwave/alerts.h"
#include "cyberwave/assets.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/edges.h"
#include "cyberwave/environments.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/projects.h"
#include "cyberwave/scene.h"
#include "cyberwave/twin.h"
#include "cyberwave/twins.h"
#include "cyberwave/workflows.h"
#include "cyberwave/workspaces.h"

#include "CppRestOpenAPIClient/ApiClient.h"
#include "CppRestOpenAPIClient/ApiConfiguration.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"

#include <cpprest/details/basic_types.h>
#include <stdexcept>
#include <utility>

namespace cyberwave
{

struct Client::RestState
{
    std::shared_ptr<org::openapitools::client::api::ApiConfiguration> config;
    std::shared_ptr<org::openapitools::client::api::ApiClient> api_client;
    std::shared_ptr<org::openapitools::client::api::DefaultApi> default_api;
};

Client::Client(const Config& config) : config_(config)
{
    if (config_.api_key.empty())
        return;
    rest_ = std::make_unique<RestState>();
    rest_->config = std::make_shared<org::openapitools::client::api::ApiConfiguration>();
    rest_->config->setBaseUrl(utility::conversions::to_string_t(config_.base_url));
    rest_->config->setApiKey(utility::conversions::to_string_t("CustomTokenAuthentication"),
                             utility::conversions::to_string_t("Bearer " + config_.api_key));
    rest_->api_client = std::make_shared<org::openapitools::client::api::ApiClient>(rest_->config);
    rest_->default_api = std::make_shared<org::openapitools::client::api::DefaultApi>(rest_->api_client);
}

Client::Client(Config&& config) : config_(std::move(config))
{
    if (config_.api_key.empty())
        return;
    rest_ = std::make_unique<RestState>();
    rest_->config = std::make_shared<org::openapitools::client::api::ApiConfiguration>();
    rest_->config->setBaseUrl(utility::conversions::to_string_t(config_.base_url));
    rest_->config->setApiKey(utility::conversions::to_string_t("CustomTokenAuthentication"),
                             utility::conversions::to_string_t("Bearer " + config_.api_key));
    rest_->api_client = std::make_shared<org::openapitools::client::api::ApiClient>(rest_->config);
    rest_->default_api = std::make_shared<org::openapitools::client::api::DefaultApi>(rest_->api_client);
}

Client::~Client() = default;

org::openapitools::client::api::DefaultApi* ClientAccess::default_api(const Client* client)
{
    return client && client->rest_ && client->rest_->default_api ? client->rest_->default_api.get() : nullptr;
}

Twin Client::twin(const std::string& slug) const { return Twin(*this, slug, slug); }

WorkspaceManager Client::workspaces() const { return WorkspaceManager(*this); }

EdgeManager Client::edges() const { return EdgeManager(*this); }

ProjectManager Client::projects() const { return ProjectManager(*this); }

EnvironmentManager Client::environments() const { return EnvironmentManager(*this); }

AssetManager Client::assets() const { return AssetManager(*this); }

TwinManager Client::twins() const { return TwinManager(*this); }

WorkflowManager Client::workflows() const { return WorkflowManager(*this); }

WorkflowRunManager Client::workflow_runs() const { return WorkflowRunManager(*this); }

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
        {"simulation", "sim"}, {"sim", "sim"},   {"real-world", "tele"},
        {"real", "tele"},      {"tele", "tele"}, {"teleoperation", "tele"},
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
    config_.source_type = it->second;
    return *this;
}

void Client::set_mqtt_client(std::shared_ptr<IMqttClient> mqtt) { mqtt_ = std::move(mqtt); }

} // namespace cyberwave
