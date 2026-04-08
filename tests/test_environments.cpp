/**
 * Symmetric with Python: client.environments, EnvironmentManager, Environment view.
 */
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/environments.h>
#include <cyberwave/exceptions.h>

#include <CppRestOpenAPIClient/model/AttachmentSchema.h>
#include <CppRestOpenAPIClient/model/EnvironmentSchema.h>
#include <cpprest/http_listener.h>

#include <atomic>
#include <cassert>
#include <mutex>
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

std::atomic<int> TestHttpServer::next_port_{32180};

} // namespace

static void test_client_has_environments()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    (void)e;
    assert(true);
}

static void test_environment_view_getters()
{
    auto schema = std::make_shared<org::openapitools::client::model::EnvironmentSchema>();
    schema->setUuid(utility::conversions::to_string_t("env-uuid"));
    schema->setName(utility::conversions::to_string_t("My Env"));
    schema->setDescription(utility::conversions::to_string_t("Description"));
    schema->setProjectUuid(utility::conversions::to_string_t("proj-uuid"));
    schema->setWorkspaceUuid(utility::conversions::to_string_t("ws-uuid"));
    Environment env(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(env.uuid() == "env-uuid");
    assert(env.name() == "My Env");
    assert(env.description() == "Description");
    assert(env.project_uuid() == "proj-uuid");
    assert(env.workspace_uuid() == "ws-uuid");
}

static void test_attachment_view_getters()
{
    auto schema = std::make_shared<org::openapitools::client::model::AttachmentSchema>();
    schema->setUuid(utility::conversions::to_string_t("att-uuid"));
    schema->setFileUrl(utility::conversions::to_string_t("https://example.com/file.png"));
    Attachment attachment(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(attachment.uuid() == "att-uuid");
    assert(attachment.file_url() == "https://example.com/file.png");
    assert(attachment.metadata_json() == "{}");
}

static void test_environments_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.list();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** get_first_or_none throws without api key (no backend available) */
static void test_environments_get_first_or_none_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        auto result = e.get_first_or_none();
        (void)result;
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** get_universal_schema_json throws without api key */
static void test_environments_get_universal_schema_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.get_universal_schema_json("some-env-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** export_urdf_scene throws without api key */
static void test_environments_export_urdf_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.export_urdf_scene("some-env-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** export_mujoco_scene throws without api key */
static void test_environments_export_mujoco_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.export_mujoco_scene("some-env-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_environments_create_preview_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.create_preview("some-env-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_environments_set_universal_schema_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.set_universal_schema("some-env-uuid", "{\"links\":[]}");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_environments_patch_universal_schema_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    EnvironmentManager e = c.environments();
    bool threw = false;
    try
    {
        e.patch_universal_schema("some-env-uuid", "/links", "[]", "replace");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_environment_exports_use_direct_routes()
{
    std::mutex mutex;
    std::vector<std::string> request_paths;
    std::vector<std::string> auth_headers;

    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            const auto auth = request.headers().find(to_utility("Authorization"));

            {
                std::lock_guard<std::mutex> lock(mutex);
                request_paths.push_back(path);
                auth_headers.push_back(auth == request.headers().end() ? "" : to_std(auth->second));
            }

            if (path == "/api/v1/environments/env-123/universal-schema.json")
            {
                request.reply(web::http::status_codes::OK, to_utility("{\"name\":\"env-123\"}"),
                              to_utility("application/json"));
                return;
            }
            if (path == "/api/v1/environments/env-123/urdf-scene.zip")
            {
                request.reply(web::http::status_codes::OK, to_utility("URDFZIP"), to_utility("application/zip"));
                return;
            }
            if (path == "/api/v1/environments/env-123/mujoco-scene.zip")
            {
                request.reply(web::http::status_codes::OK, to_utility("MUJOCOZIP"), to_utility("application/zip"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "token";
    Client client(cfg);
    EnvironmentManager environments = client.environments();

    assert(environments.get_universal_schema_json("env-123") == "{\"name\":\"env-123\"}");

    const auto urdf = environments.export_urdf_scene("env-123");
    const auto mujoco = environments.export_mujoco_scene("env-123");

    assert(std::string(urdf.begin(), urdf.end()) == "URDFZIP");
    assert(std::string(mujoco.begin(), mujoco.end()) == "MUJOCOZIP");

    std::lock_guard<std::mutex> lock(mutex);
    assert(request_paths.size() == 3);
    assert(request_paths[0] == "/api/v1/environments/env-123/universal-schema.json");
    assert(request_paths[1] == "/api/v1/environments/env-123/urdf-scene.zip");
    assert(request_paths[2] == "/api/v1/environments/env-123/mujoco-scene.zip");
    assert(auth_headers[0] == "Bearer token");
    assert(auth_headers[1] == "Bearer token");
    assert(auth_headers[2] == "Bearer token");
}

int main()
{
    test_client_has_environments();
    test_environment_view_getters();
    test_attachment_view_getters();
    test_environments_list_throws_without_api_key();
    test_environments_get_first_or_none_throws_without_api_key();
    test_environments_get_universal_schema_throws_without_api_key();
    test_environments_export_urdf_throws_without_api_key();
    test_environments_export_mujoco_throws_without_api_key();
    test_environments_create_preview_throws_without_api_key();
    test_environments_set_universal_schema_throws_without_api_key();
    test_environments_patch_universal_schema_throws_without_api_key();
    test_environment_exports_use_direct_routes();
    return 0;
}
