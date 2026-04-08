#include "cyberwave/manifest.h"

#include "cyberwave/exceptions.h"

#include <set>

namespace cyberwave
{

namespace
{

const std::set<std::string>& known_manifest_fields()
{
    static const std::set<std::string> fields = {
        "version",
        "name",
        "install",
        "install_script",
        "requirements",
        "models",
        "inference",
        "training",
        "simulate",
        "workers",
        "input",
        "gpu",
        "runtime",
        "model",
        "profile_slug",
        "heartbeat_interval",
        "upload_results",
        "results_folder",
        "resources",
        "mqtt_host",
        "mqtt_port",
        "mqtt_use_tls",
        "mqtt_tls_ca_certs",
        "mqtt_username",
        "mqtt_password",
    };
    return fields;
}

template <typename T>
std::optional<T> get_optional(const nlohmann::json& json, const char* key)
{
    if (!json.contains(key) || json.at(key).is_null())
        return std::nullopt;
    return json.at(key).get<T>();
}

} // namespace

ManifestSchema ManifestSchema::from_json(const nlohmann::json& json)
{
    if (!json.is_object())
        throw CyberwaveError("Manifest must be a JSON object");

    for (auto it = json.begin(); it != json.end(); ++it)
    {
        if (known_manifest_fields().count(it.key()) == 0)
            throw CyberwaveError("Unknown manifest field: " + it.key());
    }

    ManifestSchema manifest;
    if (json.contains("version"))
        manifest.version = json.at("version").get<std::string>();
    manifest.name = get_optional<std::string>(json, "name");
    manifest.install = get_optional<std::string>(json, "install");
    manifest.install_script = get_optional<std::string>(json, "install_script");
    manifest.requirements = get_optional<std::vector<std::string>>(json, "requirements");
    manifest.models = get_optional<std::vector<std::string>>(json, "models");
    manifest.inference = get_optional<std::string>(json, "inference");
    manifest.training = get_optional<std::string>(json, "training");
    manifest.simulate = get_optional<std::string>(json, "simulate");
    manifest.workers = get_optional<std::vector<std::string>>(json, "workers");
    manifest.runtime = get_optional<std::string>(json, "runtime");
    manifest.model = get_optional<std::string>(json, "model");
    if (json.contains("profile_slug") && !json.at("profile_slug").is_null())
        manifest.profile_slug = json.at("profile_slug").get<std::string>();
    if (json.contains("heartbeat_interval") && !json.at("heartbeat_interval").is_null())
        manifest.heartbeat_interval = json.at("heartbeat_interval").get<int>();
    if (json.contains("upload_results") && !json.at("upload_results").is_null())
        manifest.upload_results = json.at("upload_results").get<bool>();
    if (json.contains("results_folder") && !json.at("results_folder").is_null())
        manifest.results_folder = json.at("results_folder").get<std::string>();
    if (json.contains("gpu") && !json.at("gpu").is_null())
        manifest.gpu = json.at("gpu").get<bool>();

    if (json.contains("input") && !json.at("input").is_null())
    {
        if (json.at("input").is_string())
            manifest.input = std::vector<std::string>{json.at("input").get<std::string>()};
        else
            manifest.input = json.at("input").get<std::vector<std::string>>();
    }

    if (json.contains("resources") && !json.at("resources").is_null())
    {
        const auto& resources = json.at("resources");
        ResourcesSchema schema;
        schema.memory = get_optional<std::string>(resources, "memory");
        schema.cpus = get_optional<double>(resources, "cpus");
        manifest.resources = schema;
    }

    manifest.mqtt_host = get_optional<std::string>(json, "mqtt_host");
    manifest.mqtt_port = get_optional<int>(json, "mqtt_port");
    manifest.mqtt_use_tls = get_optional<bool>(json, "mqtt_use_tls");
    manifest.mqtt_tls_ca_certs = get_optional<std::string>(json, "mqtt_tls_ca_certs");
    manifest.mqtt_username = get_optional<std::string>(json, "mqtt_username");
    manifest.mqtt_password = get_optional<std::string>(json, "mqtt_password");

    manifest.validate();
    return manifest;
}

void ManifestSchema::validate() const
{
    if (version != MANIFEST_VERSION)
        throw CyberwaveError("Unsupported manifest version '" + version + "'");
    if (runtime && !model && !inference)
    {
        throw CyberwaveError("'runtime' is set but neither 'model' nor 'inference' is provided");
    }
}

std::optional<std::string> ManifestSchema::effective_install() const
{
    if (install)
        return install;
    return install_script;
}

std::string detect_dispatch_mode(const std::string& value)
{
    const auto begin = value.find_first_not_of(" \t\n\r");
    if (begin == std::string::npos)
        return "shell";
    const auto end = value.find_last_not_of(" \t\n\r");
    const std::string stripped = value.substr(begin, end - begin + 1);
    return (stripped.size() >= 3 && stripped.rfind(".py") == stripped.size() - 3 &&
            stripped.find(' ') == std::string::npos)
               ? "module"
               : "shell";
}

} // namespace cyberwave
