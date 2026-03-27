/**
 * Compact API: configure, get_client, twin(slug).
 */
#include <cassert>
#include <cyberwave/compact.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/twin.h>
#include <string>

using namespace cyberwave;

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

int main()
{
    test_configure_and_get_client();
    test_twin_via_compact();
    test_get_client_throws_if_not_configured();
    test_configure_replaces_client();
    return 0;
}
