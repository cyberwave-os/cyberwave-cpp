/**
 * Symmetric with Python: client.get_scene(environment_id), Scene get_twins, add_twin, dock, undock.
 */
#include <cpprest/http_listener.h>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/scene.h>
#include <cyberwave/twin.h>

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

std::atomic<int> TestHttpServer::next_port_{32300};

} // namespace

static void test_get_scene_returns_scene()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Scene sc = c.get_scene("env-uuid-1");
    assert(sc.environment_id() == "env-uuid-1");
}

static void test_scene_get_twins_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Scene sc = c.get_scene("env-uuid");
    bool threw = false;
    try
    {
        sc.get_twins();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_scene_add_twin_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Scene sc = c.get_scene("env-uuid");
    bool threw = false;
    try
    {
        sc.add_twin("asset-uuid", "my_twin");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_scene_refresh_noop()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Scene sc = c.get_scene("env-uuid");
    sc.refresh();
    assert(true);
}

/** add_twin with position, orientation, fixed_base throws without API key */
static void test_scene_add_twin_with_pose_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Scene sc = c.get_scene("env-uuid");
    bool threw = false;
    try
    {
        sc.add_twin("asset-uuid", "my_twin", "desc", {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0, 0.0}, true);
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Mirrors TestScene: dock throws without API key */
static void test_scene_dock_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Scene sc = c.get_scene("env-uuid");
    bool threw = false;
    try
    {
        sc.dock("child-twin-uuid", "parent-twin-uuid", "base_link");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** dock with offset position/rotation throws without API key */
static void test_scene_dock_with_offsets_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Scene sc = c.get_scene("env-uuid");
    bool threw = false;
    try
    {
        sc.dock("child-twin-uuid", "parent-twin-uuid", "base_link", {0.1, 0.2, 0.3}, {1.0, 0.0, 0.0, 0.0});
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Mirrors TestScene: undock throws without API key */
static void test_scene_undock_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Scene sc = c.get_scene("env-uuid");
    bool threw = false;
    try
    {
        sc.undock("twin-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Scene environment_id accessible after construction */
static void test_scene_environment_id_matches()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Scene sc1 = c.get_scene("env-abc");
    Scene sc2 = c.get_scene("env-xyz");
    assert(sc1.environment_id() == "env-abc");
    assert(sc2.environment_id() == "env-xyz");
}

static void test_scene_get_composed_schema_uses_canonical_route()
{
    std::mutex mutex;
    std::vector<std::string> request_paths;

    TestHttpServer server(
        [&](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            {
                std::lock_guard<std::mutex> lock(mutex);
                request_paths.push_back(path);
            }

            if (path == "/api/v1/environments/env-123/universal-schema.json")
            {
                request.reply(web::http::status_codes::OK, to_utility("{\"scene\":\"ok\"}"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "token";
    Client c(cfg);

    Scene sc = c.get_scene("env-123");
    assert(sc.get_composed_schema() == "{\"scene\":\"ok\"}");
    assert(request_paths.size() == 1);
    assert(request_paths.front() == "/api/v1/environments/env-123/universal-schema.json");
}

int main()
{
    test_get_scene_returns_scene();
    test_scene_get_twins_throws_without_api_key();
    test_scene_add_twin_throws_without_api_key();
    test_scene_refresh_noop();
    // new symmetric tests
    test_scene_add_twin_with_pose_throws();
    test_scene_dock_throws_without_api_key();
    test_scene_dock_with_offsets_throws();
    test_scene_undock_throws_without_api_key();
    test_scene_environment_id_matches();
    test_scene_get_composed_schema_uses_canonical_route();
    return 0;
}
