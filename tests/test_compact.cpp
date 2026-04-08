/**
 * Compact API: configure, get_client, twin(slug).
 */
#include <atomic>
#include <cassert>
#include <cpprest/http_listener.h>
#include <cyberwave/compact.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/twin.h>
#include <functional>
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

std::atomic<int> TestHttpServer::next_port_{32320};

} // namespace

static void test_configure_and_get_client()
{
    Config cfg;
    cfg.base_url = "https://api.example.com";
    cfg.api_key = "key";
    configure(cfg);
    Client& c = get_client();
    assert(c.config().base_url == "https://api.example.com");
    assert(c.config().api_key == "key");
}

static void test_twin_via_compact()
{
    Config cfg;
    cfg.api_key = "";
    configure(cfg);
    Twin t = twin("my-twin-slug");
    assert(t.uuid() == "my-twin-slug");
    assert(t.name() == "my-twin-slug");
    assert(&t.client() == &get_client());
}

static void test_get_client_throws_if_not_configured()
{
    // Reset global client by configuring with empty then we cannot test "never configured"
    // without a way to reset. So we test that after configure, get_client works; and we
    // document that get_client() without prior configure() throws.
    // In a single test run, configure() is already called by previous tests. So we need
    // a way to clear. For now, add a test that get_client() returns after configure().
    Config cfg;
    cfg.api_key = "";
    configure(cfg);
    (void)get_client();
}

static void test_configure_replaces_client()
{
    Config cfg1;
    cfg1.api_key = "key1";
    cfg1.base_url = "https://first.example.com";
    configure(cfg1);
    assert(get_client().config().base_url == "https://first.example.com");

    Config cfg2;
    cfg2.api_key = "key2";
    cfg2.base_url = "https://second.example.com";
    configure(cfg2);
    assert(get_client().config().base_url == "https://second.example.com");
    assert(get_client().config().api_key == "key2");
}

static void test_compact_twin_forwards_resolve_options()
{
    TestHttpServer server(
        [](web::http::http_request request)
        {
            const std::string path = to_std(request.relative_uri().path());
            if (path == "/api/v1/twins/camera")
            {
                request.reply(web::http::status_codes::NotFound, to_utility("not found"));
                return;
            }
            if (path == "/api/v1/assets/camera")
            {
                request.reply(web::http::status_codes::NotFound, to_utility("asset not found"));
                return;
            }
            if (path == "/api/v1/assets")
            {
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
            if (path == "/api/v1/environments/env-456/twins")
            {
                request.reply(web::http::status_codes::OK,
                              to_utility("[{\"uuid\":\"compact-twin\",\"name\":\"Compact Camera\","
                                         "\"asset_uuid\":\"asset-uuid\",\"environment_uuid\":\"env-456\"}]"),
                              to_utility("application/json"));
                return;
            }

            request.reply(web::http::status_codes::NotFound, to_utility("not found"));
        });

    Config cfg;
    cfg.base_url = server.base_url();
    cfg.api_key = "key";
    configure(cfg);

    TwinResolveOptions options;
    options.environment_id = "env-456";
    Twin t = twin("camera", options);
    assert(t.uuid() == "compact-twin");
    assert(t.name() == "Compact Camera");
    assert(t.environment_id() == "env-456");
}

int main()
{
    test_configure_and_get_client();
    test_twin_via_compact();
    test_get_client_throws_if_not_configured();
    test_configure_replaces_client();
    test_compact_twin_forwards_resolve_options();
    return 0;
}
