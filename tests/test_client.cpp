/**
 * Symmetric with Python: Client from config, disconnect, affect.
 */
#include "../src/cyberwave/client_internal.h"

#include <cassert>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/mqtt_interface.h>
#include <cyberwave/twin.h>
#include <memory>
#include <stdexcept>
#include <string>

using namespace cyberwave;

struct MockMqttClient : IMqttClient
{
    bool disconnect_called = false;
    bool connected = true;

    bool is_connected() const override { return connected; }
    std::string get_topic_prefix() const override { return ""; }
    void update_joint_state(const std::string&, const std::string&, double) override {}
    void publish(const std::string&, const std::string&) override {}
    void subscribe(const std::string&, MqttMessageHandler) override {}
    void disconnect() override
    {
        disconnect_called = true;
        connected = false;
    }
};

static void test_construct_from_config()
{
    Config cfg;
    cfg.base_url = "https://api.example.com";
    cfg.api_key = "key";
    Client c(cfg);
    assert(c.config().base_url == "https://api.example.com");
    assert(c.config().api_key == "key");
}

static void test_twin_stub()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("my-twin-uuid");
    assert(t.uuid() == "my-twin-uuid");
    assert(t.name() == "my-twin-uuid");
    assert(&t.client() == &c);
}

static void test_disconnect()
{
    Config cfg;
    cfg.api_key = "key";
    Client c(cfg);
    auto mqtt = std::make_shared<MockMqttClient>();
    c.set_mqtt_client(mqtt);
    auto* api_before = ClientAccess::default_api(&c);
    assert(api_before != nullptr);
    c.disconnect();
    assert(mqtt->disconnect_called);
    assert(!c.mqtt_client());
    assert(ClientAccess::default_api(&c) == api_before);
    Twin t = c.twin("x");
    assert(t.uuid() == "x");
}

static void test_affect_simulation()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    c.affect("simulation");
    assert(c.source_type() == "sim");
    c.affect("sim");
    assert(c.source_type() == "sim");
}

static void test_affect_real_world()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    c.affect("real-world");
    assert(c.source_type() == "tele");
    c.affect("real");
    assert(c.source_type() == "tele");
    c.affect("tele");
    assert(c.source_type() == "tele");
    c.affect("teleoperation");
    assert(c.source_type() == "tele");
}

static void test_affect_chain()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    c.affect("sim").affect("real-world");
    assert(c.source_type() == "tele");
}

static void test_affect_unknown_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    bool threw = false;
    try
    {
        c.affect("bogus");
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_source_type_default()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    assert(c.source_type() == "edge");
}

int main()
{
    test_construct_from_config();
    test_twin_stub();
    test_disconnect();
    test_affect_simulation();
    test_affect_real_world();
    test_affect_chain();
    test_affect_unknown_throws();
    test_source_type_default();
    return 0;
}
