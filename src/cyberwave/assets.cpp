#include "cyberwave/assets.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"
#include "rest_helpers.h"

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/HttpContent.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "CppRestOpenAPIClient/model/AssetCreateSchema.h"
#include "CppRestOpenAPIClient/model/AssetGLBFromAttachmentSchema.h"
#include "CppRestOpenAPIClient/model/AssetListSchema.h"
#include "CppRestOpenAPIClient/model/AssetSchema.h"
#include "CppRestOpenAPIClient/model/AssetUpdateSchema.h"
#include "CppRestOpenAPIClient/model/AttachmentCreateSchema.h"
#include "CppRestOpenAPIClient/model/AttachmentSchema.h"
#include "CppRestOpenAPIClient/model/CompleteLargeUploadSchema.h"
#include "CppRestOpenAPIClient/model/InitiateLargeUploadResponse.h"
#include "CppRestOpenAPIClient/model/InitiateLargeUploadSchema.h"
#include "CppRestOpenAPIClient/model/UniversalSchemaPatchSchema.h"

#include <boost/optional.hpp>
#include <cpprest/details/basic_types.h>
#include <cpprest/http_client.h>
#include <cpprest/json.h>
#include <pplx/pplxtasks.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

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

static std::shared_ptr<org::openapitools::client::model::AssetSchema>
full_asset_schema(const std::shared_ptr<void>& schema)
{
    return std::static_pointer_cast<org::openapitools::client::model::AssetSchema>(schema);
}

static std::shared_ptr<org::openapitools::client::model::AssetListSchema>
list_asset_schema(const std::shared_ptr<void>& schema)
{
    return std::static_pointer_cast<org::openapitools::client::model::AssetListSchema>(schema);
}

static bool is_not_found_error(const CyberwaveAPIError& error)
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

constexpr std::uintmax_t MAX_STANDARD_UPLOAD_BYTES = 32ull * 1024ull * 1024ull;

static std::string api_exception_message(const org::openapitools::client::api::ApiException& error)
{
    std::string message = to_std(utility::conversions::to_string_t(error.what()));
    if (auto content = error.getContent())
    {
        std::ostringstream body;
        body << content->rdbuf();
        const std::string body_text = body.str();
        if (!body_text.empty())
            message += ": " + body_text;
    }
    return message;
}

static bool is_payload_too_large_error(const org::openapitools::client::api::ApiException& error)
{
    std::string details = api_exception_message(error);
    std::transform(details.begin(), details.end(), details.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return details.find("payload too large") != std::string::npos ||
           details.find("file size exceeds maximum limit") != std::string::npos ||
           details.find("too large") != std::string::npos;
}

static std::string guess_content_type(const std::string& file_path)
{
    const auto extension = std::filesystem::path(file_path).extension().string();
    if (extension == ".glb")
        return "model/gltf-binary";
    return "application/octet-stream";
}

static std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>
attachment_upload_metadata(const std::string& filename)
{
    web::json::value metadata = web::json::value::object();
    metadata[detail::to_utility("type")] = web::json::value::string(detail::to_utility("asset_glb_upload"));
    metadata[detail::to_utility("original_filename")] = web::json::value::string(from_std(filename));
    return detail::json_object_to_any_map(metadata);
}

static void upload_file_to_signed_url(const std::string& upload_url, const std::string& file_path,
                                      const std::string& content_type)
{
    std::ifstream stream(file_path, std::ios::binary);
    if (!stream)
        throw CyberwaveError("Cannot open file: " + file_path);

    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    web::http::client::http_client client(detail::to_utility(upload_url));
    web::http::http_request request(web::http::methods::PUT);
    request.headers().set_content_type(from_std(content_type));
    request.set_body(bytes);

    auto response = client.request(request).get();
    if (response.status_code() >= web::http::status_codes::BadRequest)
    {
        throw CyberwaveAPIError("Signed upload failed with status code " + std::to_string(response.status_code()),
                                static_cast<int>(response.status_code()));
    }
}

static Asset upload_glb_direct(org::openapitools::client::api::DefaultApi* api_client, const std::string& asset_id,
                               const std::string& file_path)
{
    auto stream = std::make_shared<std::ifstream>(file_path, std::ios::binary);
    if (!*stream)
        throw CyberwaveError("Cannot open file: " + file_path);

    auto content = std::make_shared<org::openapitools::client::model::HttpContent>();
    content->setName(from_std("file"));
    content->setFileName(from_std(basename(file_path)));
    content->setContentType(from_std(guess_content_type(file_path)));
    content->setData(std::shared_ptr<std::istream>(stream, static_cast<std::istream*>(stream.get())));

    auto result = api_client->srcAppApiAssetsUploadGlb(from_std(asset_id), content).get();
    if (!result)
        throw CyberwaveError("Upload GLB returned no data");
    return Asset::from_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
}

static Asset upload_glb_via_attachment(org::openapitools::client::api::DefaultApi* api_client,
                                       const std::string& asset_id, const std::string& file_path)
{
    const std::string filename = basename(file_path);
    const std::uintmax_t file_size = std::filesystem::file_size(file_path);
    const std::string content_type = guess_content_type(file_path);
    if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<int32_t>::max()))
    {
        throw CyberwaveError("GLB file is too large for the large-upload initiation payload");
    }

    auto attachment_body = std::make_shared<org::openapitools::client::model::AttachmentCreateSchema>();
    attachment_body->setAssetUuid(from_std(asset_id));
    attachment_body->setMetadata(attachment_upload_metadata(filename));

    auto attachment = api_client->srcAppApiAttachmentsCreateAttachment(attachment_body).get();
    if (!attachment || !attachment->uuidIsSet())
        throw CyberwaveError("Create attachment returned no data");

    auto initiate_body = std::make_shared<org::openapitools::client::model::InitiateLargeUploadSchema>();
    initiate_body->setFilename(from_std(filename));
    initiate_body->setContentType(from_std(content_type));
    initiate_body->setFileSize(static_cast<int32_t>(file_size));

    auto initiated =
        api_client->srcAppApiAttachmentsInitiateLargeAttachmentUpload(attachment->getUuid(), initiate_body).get();
    const std::string upload_url = initiated && initiated->uploadUrlIsSet() ? to_std(initiated->getUploadUrl()) : "";
    const std::string storage_key = initiated && initiated->storageKeyIsSet() ? to_std(initiated->getStorageKey()) : "";
    if (upload_url.empty() || storage_key.empty())
    {
        throw CyberwaveAPIError("Large upload not available: storage backend did not return a signed upload URL.");
    }

    upload_file_to_signed_url(upload_url, file_path, content_type);

    auto complete_body = std::make_shared<org::openapitools::client::model::CompleteLargeUploadSchema>();
    complete_body->setStorageKey(from_std(storage_key));
    api_client->srcAppApiAttachmentsCompleteLargeAttachmentUpload(attachment->getUuid(), complete_body).get();

    auto asset_body = std::make_shared<org::openapitools::client::model::AssetGLBFromAttachmentSchema>();
    asset_body->setAttachmentUuid(attachment->getUuid());
    auto result = api_client->srcAppApiAssetsSetGlbFromAttachment(from_std(asset_id), asset_body).get();
    if (!result)
        throw CyberwaveError("Set GLB from attachment returned no data");
    return Asset::from_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(result)));
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
        auto b = list_asset_schema(schema_);
        return b && b->descriptionIsSet() ? to_std(b->getDescription()) : "";
    }
    auto a = full_asset_schema(schema_);
    return a && a->descriptionIsSet() ? to_std(a->getDescription()) : "";
}

std::string Asset::registry_id() const
{
    if (is_list_schema_)
    {
        auto b = list_asset_schema(schema_);
        return b && b->registryIdIsSet() ? to_std(b->getRegistryId()) : "";
    }
    auto a = full_asset_schema(schema_);
    return a && a->registryIdIsSet() ? to_std(a->getRegistryId()) : "";
}

std::string Asset::registry_id_alias() const
{
    if (is_list_schema_)
    {
        auto b = list_asset_schema(schema_);
        return b && b->registryIdAliasIsSet() ? to_std(b->getRegistryIdAlias()) : "";
    }
    auto a = full_asset_schema(schema_);
    return a && a->registryIdAliasIsSet() ? to_std(a->getRegistryIdAlias()) : "";
}

bool Asset::fixed_base() const
{
    if (is_list_schema_)
    {
        auto b = list_asset_schema(schema_);
        return b && b->fixedBaseIsSet() && b->isFixedBase();
    }
    auto a = full_asset_schema(schema_);
    return a && a->fixedBaseIsSet() && a->isFixedBase();
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

    if (!std::filesystem::exists(file_path))
        throw CyberwaveError("GLB file not found: " + file_path);

    const std::uintmax_t file_size = std::filesystem::file_size(file_path);
    try
    {
        if (file_size > MAX_STANDARD_UPLOAD_BYTES)
            return upload_glb_via_attachment(a, asset_id, file_path);

        try
        {
            return upload_glb_direct(a, asset_id, file_path);
        }
        catch (const org::openapitools::client::api::ApiException& error)
        {
            if (is_payload_too_large_error(error))
                return upload_glb_via_attachment(a, asset_id, file_path);
            throw;
        }
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(api_exception_message(e), 0);
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

Asset AssetManager::get_by_registry_id(const std::string& registry_id) const
{
    try
    {
        return get(registry_id);
    }
    catch (const CyberwaveAPIError& error)
    {
        if (!is_not_found_error(error))
        {
            throw;
        }
    }

    const std::string normalized = registry_id;
    for (const auto& asset : list())
    {
        if (asset.registry_id() == normalized || asset.registry_id_alias() == normalized)
        {
            return get(asset.uuid());
        }
    }

    throw CyberwaveError("Asset not found for registry identifier: " + registry_id);
}

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

std::string AssetManager::rebuild_universal_schema(const std::string& asset_id, bool sync) const
{
    auto* a = api(client_.get());
    if (!a)
        throw CyberwaveError("Client has no REST API (missing api_key)");
    try
    {
        boost::optional<bool> sync_opt;
        if (sync)
            sync_opt = true;
        auto result = a->srcAppApiAssetsRebuildAssetUniversalSchema(from_std(asset_id), sync_opt).get();
        return asset_any_map_to_json(result);
    }
    catch (const org::openapitools::client::api::ApiException& e)
    {
        throw CyberwaveAPIError(to_std(utility::conversions::to_string_t(e.what())), 0);
    }
}

} // namespace cyberwave
