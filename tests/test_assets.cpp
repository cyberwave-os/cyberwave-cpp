/**
 * Symmetric with Python: client.assets, AssetManager, Asset view.
 */
#include <cyberwave/assets.h>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>

#include <CppRestOpenAPIClient/model/AssetListSchema.h>
#include <CppRestOpenAPIClient/model/AssetSchema.h>
#include <cpprest/http_listener.h>

#include <atomic>
#include <cassert>
#include <fstream>
#include <mutex>
#include <string>

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

std::atomic<int> TestHttpServer::next_port_{32220};

void write_sparse_file(const std::string& path, std::streamoff size)
{
    std::ofstream output(path, std::ios::binary);
    output.seekp(size - 1);
    output.put('\0');
}

} // namespace

static void test_client_has_assets()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    AssetManager a = c.assets();
    (void)a;
    assert(true);
}

static void test_asset_view_from_asset_schema()
{
    auto schema = std::make_shared<org::openapitools::client::model::AssetSchema>();
    schema->setUuid(utility::conversions::to_string_t("asset-uuid"));
    schema->setName(utility::conversions::to_string_t("My Asset"));
    schema->setDescription(utility::conversions::to_string_t("Description"));
    schema->setRegistryId(utility::conversions::to_string_t("cyberwave/my-asset"));
    schema->setRegistryIdAlias(utility::conversions::to_string_t("my-asset"));
    schema->setFixedBase(true);
    Asset asset = Asset::from_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(asset.uuid() == "asset-uuid");
    assert(asset.name() == "My Asset");
    assert(asset.description() == "Description");
    assert(asset.registry_id() == "cyberwave/my-asset");
    assert(asset.registry_id_alias() == "my-asset");
    assert(asset.fixed_base());
}

static void test_asset_view_from_asset_list_schema()
{
    auto schema = std::make_shared<org::openapitools::client::model::AssetListSchema>();
    schema->setUuid(utility::conversions::to_string_t("list-uuid"));
    schema->setName(utility::conversions::to_string_t("List Asset"));
    schema->setDescription(utility::conversions::to_string_t("List desc"));
    schema->setRegistryId(utility::conversions::to_string_t("cyberwave/list-asset"));
    schema->setRegistryIdAlias(utility::conversions::to_string_t("camera"));
    schema->setFixedBase(true);
    Asset asset = Asset::from_list_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(asset.uuid() == "list-uuid");
    assert(asset.name() == "List Asset");
    assert(asset.description() == "List desc");
    assert(asset.registry_id() == "cyberwave/list-asset");
    assert(asset.registry_id_alias() == "camera");
    assert(asset.fixed_base());
}

static void test_assets_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    AssetManager a = c.assets();
    bool threw = false;
    try
    {
        a.list();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** get_by_registry_id delegates to get() → throws without api key */
static void test_assets_get_by_registry_id_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    AssetManager a = c.assets();
    bool threw = false;
    try
    {
        a.get_by_registry_id("reg-id-123");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** get_universal_schema throws without api key */
static void test_assets_get_universal_schema_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    AssetManager a = c.assets();
    bool threw = false;
    try
    {
        a.get_universal_schema("some-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** patch_universal_schema throws without api key */
static void test_assets_patch_universal_schema_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    AssetManager a = c.assets();
    bool threw = false;
    try
    {
        a.patch_universal_schema("some-uuid", "/color", "\"red\"");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_assets_rebuild_universal_schema_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    AssetManager a = c.assets();
    bool threw = false;
    try
    {
        a.rebuild_universal_schema("some-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_assets_get_by_registry_id_falls_back_on_not_found()
{
    std::mutex mutex;
    int list_requests = 0;

    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());

            if (path == "/api/v1/assets/camera")
            {
                request.reply(web::http::status_codes::NotFound, to_utility("asset not found"));
                return;
            }
            if (path == "/api/v1/assets")
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++list_requests;
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

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "token";
    Client client(cfg);
    AssetManager assets = client.assets();

    const Asset asset = assets.get_by_registry_id("camera");
    assert(asset.uuid() == "asset-uuid");
    assert(asset.registry_id_alias() == "camera");
    assert(list_requests == 1);
}

static void test_assets_get_by_registry_id_does_not_mask_non_not_found_errors()
{
    bool listed_assets = false;

    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());

            if (path == "/api/v1/assets/broken")
            {
                request.reply(web::http::status_codes::InternalError, to_utility("server exploded"));
                return;
            }
            if (path == "/api/v1/assets")
            {
                listed_assets = true;
                request.reply(web::http::status_codes::OK, to_utility("[]"), to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "token";
    Client client(cfg);
    AssetManager assets = client.assets();

    bool threw = false;
    try
    {
        (void)assets.get_by_registry_id("broken");
    }
    catch (const CyberwaveAPIError&)
    {
        threw = true;
    }

    assert(threw);
    assert(!listed_assets);
}

static void test_assets_upload_glb_falls_back_to_attachment_on_payload_too_large()
{
    std::mutex mutex;
    int direct_upload_attempts = 0;
    int signed_upload_attempts = 0;
    int attachment_creations = 0;
    int attachment_completions = 0;
    int set_from_attachment_calls = 0;
    std::string signed_upload_auth_header;

    TestHttpServer* server_ptr = nullptr;
    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            const auto auth = request.headers().find(to_utility("Authorization"));
            const std::string auth_header = auth == request.headers().end() ? "" : to_std(auth->second);

            if (path == "/api/v1/assets/asset-1/glb-file")
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++direct_upload_attempts;
                request.reply(web::http::status_codes::RequestEntityTooLarge, to_utility("Payload Too Large"));
                return;
            }
            if (path == "/api/v1/attachments")
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++attachment_creations;
                request.reply(web::http::status_codes::OK, to_utility("{\"uuid\":\"attachment-1\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/attachments/attachment-1/initiate-large-upload")
            {
                request.reply(
                    web::http::status_codes::OK,
                    to_utility(std::string("{\"upload_url\":\"") + server_ptr->base_url() +
                               "/signed-upload?signature=abc\",\"storage_key\":\"attachments/large/model.glb\"}"),
                    to_utility("application/json"));
                return;
            }
            if (path == "/signed-upload")
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++signed_upload_attempts;
                signed_upload_auth_header = auth_header;
                request.reply(web::http::status_codes::OK, to_utility(""));
                return;
            }
            if (path == "/api/v1/attachments/attachment-1/complete-large-upload")
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++attachment_completions;
                request.reply(web::http::status_codes::OK, to_utility("{\"uuid\":\"attachment-1\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/assets/asset-1/glb-from-attachment")
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++set_from_attachment_calls;
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"asset-1\",\"name\":\"Uploaded Asset\"}"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });
    server_ptr = &server;

    const std::string file_path = "/tmp/cyberwave-small-upload.glb";
    {
        std::ofstream output(file_path, std::ios::binary);
        output << "glb-data";
    }

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "token";
    Client client(cfg);
    AssetManager assets = client.assets();

    const Asset asset = assets.upload_glb("asset-1", file_path);
    assert(asset.uuid() == "asset-1");

    std::lock_guard<std::mutex> lock(mutex);
    assert(direct_upload_attempts == 1);
    assert(attachment_creations == 1);
    assert(signed_upload_attempts == 1);
    assert(attachment_completions == 1);
    assert(set_from_attachment_calls == 1);
    assert(signed_upload_auth_header.empty());
}

static void test_assets_upload_glb_uses_attachment_flow_for_large_files()
{
    std::mutex mutex;
    int direct_upload_attempts = 0;
    int attachment_creations = 0;
    int signed_upload_attempts = 0;

    TestHttpServer* server_ptr = nullptr;
    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());

            if (path == "/api/v1/assets/asset-1/glb-file")
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++direct_upload_attempts;
                request.reply(web::http::status_codes::InternalError, to_utility("should not call direct upload"));
                return;
            }
            if (path == "/api/v1/attachments")
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++attachment_creations;
                request.reply(web::http::status_codes::OK, to_utility("{\"uuid\":\"attachment-1\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/attachments/attachment-1/initiate-large-upload")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility(std::string("{\"upload_url\":\"") + server_ptr->base_url() +
                                         "/signed-upload\",\"storage_key\":\"attachments/large/huge.glb\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/signed-upload")
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++signed_upload_attempts;
                request.reply(web::http::status_codes::OK, to_utility(""));
                return;
            }
            if (path == "/api/v1/attachments/attachment-1/complete-large-upload")
            {
                request.reply(web::http::status_codes::OK, to_utility("{\"uuid\":\"attachment-1\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/assets/asset-1/glb-from-attachment")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("{\"uuid\":\"asset-1\",\"name\":\"Uploaded Asset\"}"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });
    server_ptr = &server;

    const std::string file_path = "/tmp/cyberwave-large-upload.glb";
    write_sparse_file(file_path, (32 * 1024 * 1024) + 1);

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "token";
    Client client(cfg);
    AssetManager assets = client.assets();

    const Asset asset = assets.upload_glb("asset-1", file_path);
    assert(asset.uuid() == "asset-1");

    std::lock_guard<std::mutex> lock(mutex);
    assert(direct_upload_attempts == 0);
    assert(attachment_creations == 1);
    assert(signed_upload_attempts == 1);
}

int main()
{
    test_client_has_assets();
    test_asset_view_from_asset_schema();
    test_asset_view_from_asset_list_schema();
    test_assets_list_throws_without_api_key();
    test_assets_get_by_registry_id_throws_without_api_key();
    test_assets_get_universal_schema_throws_without_api_key();
    test_assets_patch_universal_schema_throws_without_api_key();
    test_assets_rebuild_universal_schema_throws_without_api_key();
    test_assets_get_by_registry_id_falls_back_on_not_found();
    test_assets_get_by_registry_id_does_not_mask_non_not_found_errors();
    test_assets_upload_glb_falls_back_to_attachment_on_payload_too_large();
    test_assets_upload_glb_uses_attachment_flow_for_large_files();
    return 0;
}
