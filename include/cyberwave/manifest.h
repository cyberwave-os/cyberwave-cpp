#ifndef CYBERWAVE_MANIFEST_H
#define CYBERWAVE_MANIFEST_H

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace cyberwave
{

constexpr const char* MANIFEST_VERSION = "1";

struct ResourcesSchema
{
    std::optional<std::string> memory;
    std::optional<double> cpus;
};

struct ManifestSchema
{
    std::string version{MANIFEST_VERSION};
    std::optional<std::string> name;
    std::optional<std::string> install;
    std::optional<std::string> install_script;
    std::optional<std::vector<std::string>> requirements;
    std::optional<std::vector<std::string>> models;
    std::optional<std::string> inference;
    std::optional<std::string> training;
    std::optional<std::string> simulate;
    std::optional<std::vector<std::string>> workers;
    std::optional<std::vector<std::string>> input;
    bool gpu{false};
    std::optional<std::string> runtime;
    std::optional<std::string> model;
    std::string profile_slug{"default"};
    int heartbeat_interval{30};
    bool upload_results{true};
    std::string results_folder{"/results"};
    std::optional<ResourcesSchema> resources;
    std::optional<std::string> mqtt_host;
    std::optional<int> mqtt_port;
    std::optional<bool> mqtt_use_tls;
    std::optional<std::string> mqtt_tls_ca_certs;
    std::optional<std::string> mqtt_username;
    std::optional<std::string> mqtt_password;

    static ManifestSchema from_json(const nlohmann::json& json);
    void validate() const;
    std::optional<std::string> effective_install() const;
};

std::string detect_dispatch_mode(const std::string& value);

} // namespace cyberwave

#endif // CYBERWAVE_MANIFEST_H
