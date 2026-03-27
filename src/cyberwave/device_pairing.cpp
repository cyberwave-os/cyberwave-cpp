#include "cyberwave/device_pairing.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/twins.h"

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/TwinCreateSchema.h"
#include "CppRestOpenAPIClient/model/TwinSchema.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>
#include <pplx/pplxtasks.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>

namespace cyberwave
{
namespace device_pairing
{

namespace
{

const char EDGE_CONFIGS_KEY[] = "edge_configs";
const char EDGE_FINGERPRINT_KEY[] = "edge_fingerprint";
// Python: EDGE_DEVICE_UUID_NAMESPACE = uuid.UUID("7f09e16f-ef6f-410d-9d5d-c071d8fbd1ad")
const unsigned char UUID5_NAMESPACE[16] = {0x7f, 0x09, 0xe1, 0x6f, 0xef, 0x6f, 0x41, 0x0d,
                                           0x9d, 0x5d, 0xc0, 0x71, 0xd8, 0xfb, 0xd1, 0xad};

std::string to_std(const utility::string_t& t)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return std::string(t);
#else
    return utility::conversions::to_utf8string(t);
#endif
}

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

org::openapitools::client::api::DefaultApi* api(const Client& client) { return ClientAccess::default_api(&client); }

/** UUID5(twin_id:fingerprint) to match Python. */
std::string device_uuid_for_fingerprint(const std::string& twin_id, const std::string& fingerprint)
{
    std::string name = twin_id + ":" + fingerprint;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, UUID5_NAMESPACE, 16);
    SHA1_Update(&ctx, name.data(), name.size());
    SHA1_Final(hash, &ctx);
    hash[6] = (hash[6] & 0x0f) | 0x50;
    hash[8] = (hash[8] & 0x3f) | 0x80;
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    out << std::setw(2) << (int)hash[0] << std::setw(2) << (int)hash[1] << std::setw(2) << (int)hash[2] << std::setw(2)
        << (int)hash[3] << '-' << std::setw(2) << (int)hash[4] << std::setw(2) << (int)hash[5] << '-' << std::setw(2)
        << (int)hash[6] << std::setw(2) << (int)hash[7] << '-' << std::setw(2) << (int)hash[8] << std::setw(2)
        << (int)hash[9] << '-' << std::setw(2) << (int)hash[10] << std::setw(2) << (int)hash[11] << std::setw(2)
        << (int)hash[12] << std::setw(2) << (int)hash[13] << std::setw(2) << (int)hash[14] << std::setw(2)
        << (int)hash[15];
    return out.str();
}

web::json::value
metadata_map_to_json(const std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>& m)
{
    web::json::value j = web::json::value::object();
    for (const auto& kv : m)
        j[to_std(kv.first)] = kv.second ? kv.second->toJson() : web::json::value::null();
    return j;
}

std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>
json_to_metadata_map(const web::json::value& j)
{
    std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>> out;
    if (!j.is_object())
        return out;
    for (const auto& kv : j.as_object())
    {
        auto any_val = std::make_shared<org::openapitools::client::model::AnyType>();
        if (any_val->fromJson(kv.second))
            out[kv.first] = any_val;
    }
    return out;
}

bool is_legacy_edge_configs(const web::json::value& edge_configs)
{
    if (!edge_configs.is_object() || edge_configs.as_object().empty())
        return false;
    if (edge_configs.has_field(from_std("edge_fingerprint")) || edge_configs.has_field(from_std("camera_config")))
        return false;
    for (const auto& kv : edge_configs.as_object())
        if (!kv.second.is_object())
            return false;
    return true;
}

void iter_bindings(const web::json::value& edge_configs,
                   std::function<void(const std::string& fingerprint, const web::json::value& binding)> fn)
{
    if (!edge_configs.is_object())
        return;
    if (is_legacy_edge_configs(edge_configs))
    {
        for (const auto& kv : edge_configs.as_object())
            if (kv.second.is_object())
                fn(to_std(kv.first), kv.second);
        return;
    }
    std::string fp;
    if (edge_configs.has_field(from_std("edge_fingerprint")) &&
        edge_configs.at(from_std("edge_fingerprint")).is_string())
        fp = to_std(edge_configs.at(from_std("edge_fingerprint")).as_string());
    if (!fp.empty())
        fn(fp, edge_configs);
}

PairedDevice build_device(const std::string& twin_id, const std::string& fingerprint, const web::json::value& binding)
{
    PairedDevice d;
    d.uuid = device_uuid_for_fingerprint(twin_id, fingerprint);
    d.fingerprint = fingerprint;
    d.twin_uuid = twin_id;
    d.status = "offline";
    if (binding.has_field(from_std("status_data")) && binding.at(from_std("status_data")).is_object())
        d.status = "online";
    if (binding.has_field(from_std("device_info")) && binding.at(from_std("device_info")).is_object())
    {
        const auto& di = binding.at(from_std("device_info"));
        if (di.has_field(from_std("hostname")) && di.at(from_std("hostname")).is_string())
            d.hostname = to_std(di.at(from_std("hostname")).as_string());
        if (di.has_field(from_std("platform")) && di.at(from_std("platform")).is_string())
            d.platform = to_std(di.at(from_std("platform")).as_string());
    }
    if (binding.has_field(from_std("last_sync")) && binding.at(from_std("last_sync")).is_string())
        d.last_heartbeat = d.updated_at = to_std(binding.at(from_std("last_sync")).as_string());
    if (binding.has_field(from_std("registered_at")) && binding.at(from_std("registered_at")).is_string())
        d.paired_at = to_std(binding.at(from_std("registered_at")).as_string());
    if (binding.has_field(from_std("camera_config")) && binding.at(from_std("camera_config")).is_object())
        for (const auto& kv : binding.at(from_std("camera_config")).as_object())
            d.edge_config[to_std(kv.first)] =
                kv.second.is_string() ? to_std(kv.second.as_string()) : kv.second.serialize();
    return d;
}

std::string now_iso()
{
    auto t = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
    std::time_t sec = ms / 1000;
    std::tm* tm = std::gmtime(&sec);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
    return std::string(buf) + "." + std::to_string(ms % 1000) + "Z";
}

} // namespace

std::vector<PairedDevice> list_devices(const TwinManager& twins, const std::string& twin_id)
{
    auto* a = api(twins.client());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    auto schema = a->srcAppApiTwinsGetTwin(from_std(twin_id)).get();
    if (!schema || !schema->metadataIsSet())
        return {};
    web::json::value meta = metadata_map_to_json(schema->getMetadata());
    if (!meta.has_field(from_std(EDGE_CONFIGS_KEY)) || !meta.at(from_std(EDGE_CONFIGS_KEY)).is_object())
        return {};
    web::json::value edge_configs = meta.at(from_std(EDGE_CONFIGS_KEY));
    std::vector<PairedDevice> out;
    iter_bindings(edge_configs, [&](const std::string& fp, const web::json::value& binding)
                  { out.push_back(build_device(twin_id, fp, binding)); });
    return out;
}

PairedDevice get_device(const TwinManager& twins, const std::string& twin_id, const std::string& device_uuid)
{
    auto devices = list_devices(twins, twin_id);
    for (const auto& d : devices)
        if (d.uuid == device_uuid)
            return d;
    throw CyberwaveAPIError("Device not found", 404);
}

PairedDevice pair_device(const TwinManager& twins, const std::string& twin_id, const std::string& fingerprint,
                         const std::string& hostname, const std::string& platform,
                         const std::map<std::string, std::string>& edge_config)
{
    if (fingerprint.empty())
        throw CyberwaveValidationError("Fingerprint is required");
    auto* a = api(twins.client());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");

    auto all_twins = a->srcAppApiTwinsListAllTwins().get();
    for (const auto& t : all_twins)
    {
        if (!t)
            continue;
        std::string cand_uuid = t->uuidIsSet() ? to_std(t->getUuid()) : "";
        if (cand_uuid == twin_id)
            continue;
        if (!t->metadataIsSet())
            continue;
        web::json::value cand_meta = metadata_map_to_json(t->getMetadata());
        if (!cand_meta.has_field(from_std(EDGE_CONFIGS_KEY)) || !cand_meta.at(from_std(EDGE_CONFIGS_KEY)).is_object())
            continue;
        web::json::value cand_ec = cand_meta.at(from_std(EDGE_CONFIGS_KEY));
        iter_bindings(cand_ec,
                      [&](const std::string& fp, const web::json::value&)
                      {
                          if (fp == fingerprint)
                              throw CyberwaveAPIError(
                                  "Device already paired to twin '" + cand_uuid + "'. Unpair it first.", 409);
                      });
    }

    auto schema = a->srcAppApiTwinsGetTwin(from_std(twin_id)).get();
    if (!schema)
        throw CyberwaveError("Twin not found");
    web::json::value meta =
        schema->metadataIsSet() ? metadata_map_to_json(schema->getMetadata()) : web::json::value::object();
    web::json::value edge_configs =
        (meta.has_field(from_std(EDGE_CONFIGS_KEY)) && meta.at(from_std(EDGE_CONFIGS_KEY)).is_object())
            ? meta.at(from_std(EDGE_CONFIGS_KEY))
            : web::json::value::object();

    web::json::value device_info = web::json::value::object();
    if (!hostname.empty())
        device_info[from_std("hostname")] = web::json::value::string(from_std(hostname));
    if (!platform.empty())
        device_info[from_std("platform")] = web::json::value::string(from_std(platform));
    std::string now = now_iso();
    web::json::value binding = web::json::value::object();
    binding[from_std("edge_fingerprint")] = web::json::value::string(from_std(fingerprint));
    binding[from_std("device_info")] = device_info;
    binding[from_std("registered_at")] = web::json::value::string(from_std(now));
    binding[from_std("last_sync")] = web::json::value::string(from_std(now));
    web::json::value cam = web::json::value::object();
    for (const auto& kv : edge_config)
        cam[from_std(kv.first)] = web::json::value::string(from_std(kv.second));
    binding[from_std("camera_config")] = cam;

    meta[from_std(EDGE_CONFIGS_KEY)] = binding;
    auto body = std::make_shared<org::openapitools::client::model::TwinCreateSchema>();
    body->setMetadata(json_to_metadata_map(meta));
    a->srcAppApiTwinsUpdateTwin(from_std(twin_id), body).get();
    return build_device(twin_id, fingerprint, binding);
}

std::string unpair_device(const TwinManager& twins, const std::string& twin_id, const std::string& device_uuid)
{
    auto* a = api(twins.client());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    auto schema = a->srcAppApiTwinsGetTwin(from_std(twin_id)).get();
    if (!schema)
        throw CyberwaveError("Twin not found");
    web::json::value meta =
        schema->metadataIsSet() ? metadata_map_to_json(schema->getMetadata()) : web::json::value::object();
    if (!meta.has_field(from_std(EDGE_CONFIGS_KEY)) || !meta.at(from_std(EDGE_CONFIGS_KEY)).is_object())
        throw CyberwaveAPIError("Device not found", 404);
    web::json::value edge_configs = meta.at(from_std(EDGE_CONFIGS_KEY));

    std::string removed_fp;
    if (is_legacy_edge_configs(edge_configs))
    {
        for (const auto& kv : edge_configs.as_object())
        {
            if (!kv.second.is_object())
                continue;
            std::string fp = to_std(kv.first);
            if (device_uuid_for_fingerprint(twin_id, fp) == device_uuid)
            {
                removed_fp = fp;
                break;
            }
        }
        if (removed_fp.empty())
            throw CyberwaveAPIError("Device not found", 404);
        web::json::value new_ec = web::json::value::object();
        for (const auto& kv : edge_configs.as_object())
            if (to_std(kv.first) != removed_fp)
                new_ec[kv.first] = kv.second;
        web::json::value new_meta = web::json::value::object();
        for (const auto& kv : meta.as_object())
            if (kv.first != from_std(EDGE_CONFIGS_KEY))
                new_meta[kv.first] = kv.second;
        if (!new_ec.as_object().empty())
            new_meta[from_std(EDGE_CONFIGS_KEY)] = new_ec;
        else
            new_meta[from_std(EDGE_CONFIGS_KEY)] = web::json::value::null();
        meta = new_meta;
    }
    else
    {
        std::string fp;
        if (edge_configs.has_field(from_std("edge_fingerprint")) &&
            edge_configs.at(from_std("edge_fingerprint")).is_string())
            fp = to_std(edge_configs.at(from_std("edge_fingerprint")).as_string());
        if (fp.empty() || device_uuid_for_fingerprint(twin_id, fp) != device_uuid)
            throw CyberwaveAPIError("Device not found", 404);
        removed_fp = fp;
        web::json::value new_meta = web::json::value::object();
        for (const auto& kv : meta.as_object())
            if (kv.first != from_std(EDGE_CONFIGS_KEY))
                new_meta[kv.first] = kv.second;
        new_meta[from_std(EDGE_CONFIGS_KEY)] = web::json::value::null();
        meta = new_meta;
    }

    auto body = std::make_shared<org::openapitools::client::model::TwinCreateSchema>();
    body->setMetadata(json_to_metadata_map(meta));
    a->srcAppApiTwinsUpdateTwin(from_std(twin_id), body).get();
    return "Device " + removed_fp + " unpaired";
}

} // namespace device_pairing
} // namespace cyberwave
