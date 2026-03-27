#include "cyberwave/assets.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/HttpContent.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/AssetCreateSchema.h"
#include "CppRestOpenAPIClient/model/AssetListSchema.h"
#include "CppRestOpenAPIClient/model/AssetSchema.h"
#include "CppRestOpenAPIClient/model/AssetUpdateSchema.h"
#include "CppRestOpenAPIClient/model/UniversalSchemaPatchSchema.h"

#include <boost/optional.hpp>
#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>
#include <pplx/pplxtasks.h>

#include <algorithm>
#include <fstream>

namespace cyberwave
{

namespace
{

std::string to_std(const utility::string_t& t)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return std::string(t);
#else
    return utility::conversions::to_utf8string(t);
#endif
}

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

static org::openapitools::client::api::DefaultApi* api(const Client& client)
{
    return ClientAccess::default_api(&client);
}

static std::string basename(const std::string& path)
{
    std::string::size_type pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

} // namespace

Asset Asset::from_schema(std::shared_ptr<void> schema_ptr) { return Asset(std::move(schema_ptr), false); }

Asset Asset::from_list_schema(std::shared_ptr<void> schema_ptr) { return Asset(std::move(schema_ptr), true); }

Asset::Asset(std::shared_ptr<void> schema_ptr, bool is_list_schema)
    : schema_(std::move(schema_ptr)), is_list_schema_(is_list_schema)
{
}

std::string Asset::uuid() const
{
    if (is_list_schema_)
    {
        auto b = std::static_pointer_cast<org::openapitools::client::model::AssetListSchema>(schema_);
        return b && b->uuidIsSet() ? to_std(b->getUuid()) : "";
    }
    auto a = std::static_pointer_cast<org::openapitools::client::model::AssetSchema>(schema_);
    return a && a->uuidIsSet() ? to_std(a->getUuid()) : "";
}

std::string Asset::name() const
{
    if (is_list_schema_)
    {
        auto b = std::static_pointer_cast<org::openapitools::client::model::AssetListSchema>(schema_);
        return b && b->nameIsSet() ? to_std(b->getName()) : "";
    }
    auto a = std::static_pointer_cast<org::openapitools::client::model::AssetSchema>(schema_);
    return a && a->nameIsSet() ? to_std(a->getName()) : "";
}

std::string Asset::description() const
{
    if (is_list_schema_)
    {
        auto b = std::static_pointer_cast<org::openapitools::client::model::AssetListSchema>(schema_);
        return b && b->descriptionIsSet() ? to_std(b->getDescription()) : "";
    }
    auto a = std::static_pointer_cast<org::openapitools::client::model::AssetSchema>(schema_);
    return a && a->descriptionIsSet() ? to_std(a->getDescription()) : "";
}

AssetManager::AssetManager(const Client& client) : client_(client) {}

std::vector<Asset> AssetManager::list(const std::string& workspace_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto vec = a->srcAppApiAssetsListAssets().get();
        std::vector<Asset> out;
        for (auto& ptr : vec)
        {
            if (ptr)
                out.push_back(Asset::from_list_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(ptr))));
        }
        return out;
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Asset AssetManager::get(const std::string& asset_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiAssetsGetAsset(from_std(asset_id)).get();
        if (!result)
            throw CyberwaveError("Get asset returned no data");
        return Asset::from_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Asset AssetManager::create(const std::string& name, const std::string& description) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::AssetCreateSchema>();
        body->setName(from_std(name));
        if (!description.empty())
            body->setDescription(from_std(description));
        auto result = a->srcAppApiAssetsCreateAsset(body).get();
        if (!result)
            throw CyberwaveError("Create asset returned no data");
        return Asset::from_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Asset AssetManager::update(const std::string& asset_id, const std::string& name, const std::string& description) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::AssetUpdateSchema>();
        if (!name.empty())
            body->setName(from_std(name));
        if (!description.empty())
            body->setDescription(from_std(description));
        auto result = a->srcAppApiAssetsUpdateAsset(from_std(asset_id), body).get();
        if (!result)
            throw CyberwaveError("Update asset returned no data");
        return Asset::from_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

void AssetManager::delete_asset(const std::string& asset_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        a->srcAppApiAssetsDeleteAsset(from_std(asset_id)).get();
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

Asset AssetManager::upload_glb(const std::string& asset_id, const std::string& file_path) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    auto stream = std::make_shared<std::ifstream>(file_path, std::ios::binary);
    if (!*stream)
        throw CyberwaveError("Cannot open file: " + file_path);
    try
    {
        auto content = std::make_shared<org::openapitools::client::model::HttpContent>();
        content->setName(from_std("file"));
        content->setFileName(from_std(basename(file_path)));
        content->setContentType(from_std("model/gltf-binary"));
        content->setData(std::shared_ptr<std::istream>(stream, static_cast<std::istream*>(stream.get())));
        auto result = a->srcAppApiAssetsUploadGlb(from_std(asset_id), content).get();
        if (!result)
            throw CyberwaveError("Upload GLB returned no data");
        return Asset::from_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

static std::string
asset_any_map_to_json(const std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>& m)
{
    web::json::value obj = web::json::value::object();
    for (const auto& kv : m)
    {
        if (kv.second)
            obj[kv.first] = kv.second->toJson();
    }
#if defined(_TURN_OFF_PLATFORM_STRING)
    return obj.serialize();
#else
    return utility::conversions::to_utf8string(obj.serialize());
#endif
}

Asset AssetManager::get_by_registry_id(const std::string& registry_id) const { return get(registry_id); }

std::string AssetManager::get_universal_schema(const std::string& asset_id) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto result = a->srcAppApiAssetsGetAssetUniversalSchema(from_std(asset_id)).get();
        return asset_any_map_to_json(result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::string AssetManager::get_universal_schema_at_path(const std::string& asset_id, const std::string& path) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        boost::optional<utility::string_t> path_opt;
        if (!path.empty())
            path_opt = from_std(path);
        auto result = a->srcAppApiAssetsGetAssetUniversalSchemaAtPath(from_std(asset_id), path_opt).get();
        return asset_any_map_to_json(result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

std::string AssetManager::patch_universal_schema(const std::string& asset_id, const std::string& path,
                                                 const std::string& value_json, const std::string& op) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        auto body = std::make_shared<org::openapitools::client::model::UniversalSchemaPatchSchema>();
        body->setOp(from_std(op));
        body->setPath(from_std(path));
        web::json::value val = web::json::value::parse(from_std(value_json));
        auto any_val = std::make_shared<org::openapitools::client::model::AnyType>();
        any_val->fromJson(val);
        body->setValue(any_val);
        auto result = a->srcAppApiAssetsPatchAssetUniversalSchema(from_std(asset_id), body).get();
        return asset_any_map_to_json(result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
