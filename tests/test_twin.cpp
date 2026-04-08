/**
 * Symmetric with Python test_twin_latest_frame.py and Twin method tests.
 *
 * Python test_twin_latest_frame.py tests twin.get_latest_frame() (REST API),
 * capture_frame() delegation, and error wrapping.
 * Here we test Twin edit helpers and handle accessors without a live backend.
 */

#include <cyberwave/alerts.h>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/joints.h>
#include <cyberwave/motion.h>
#include <cyberwave/navigation.h>
#include <cyberwave/twin.h>

#include <CppRestOpenAPIClient/model/TwinSchema.h>

#include <cassert>
#include <string>

using namespace cyberwave;

/** Mirrors test_twin_get_latest_frame_delegates_to_manager: twin.alerts() returns TwinAlertManager */
static void test_twin_get_alerts_handle()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "twin-uuid", "Twin A");
    TwinAlertManager am = t.alerts();
    (void)am;
    assert(true);
}

/** twin.navigation() returns TwinNavigationHandle with correct twin_uuid */
static void test_twin_get_navigation_handle()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "twin-nav", "Nav Twin");
    TwinNavigationHandle nav = t.navigation();
    assert(nav.twin_uuid() == "twin-nav");
}

/** twin.motion() returns TwinMotionHandle with correct twin_uuid */
static void test_twin_get_motion_handle()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "twin-motion", "Motion Twin");
    TwinMotionHandle mot = t.motion();
    assert(mot.twin_uuid() == "twin-motion");
}

/** twin.joints() returns JointController */
static void test_twin_get_joints_handle()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "twin-joints", "Joint Twin");
    JointController jc = t.joints();
    (void)jc;
    assert(true);
}

/** Mirrors test_twin_get_latest_frame_wraps_errors:
 *  edit_position() calls REST → throws CyberwaveError without API key */
static void test_twin_edit_position_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "twin-uuid", "Twin");
    bool threw = false;
    try
    {
        t.edit_position(1.0, 2.0, 3.0);
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** edit_rotation() calls REST → throws CyberwaveError without API key */
static void test_twin_edit_rotation_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "twin-uuid", "Twin");
    bool threw = false;
    try
    {
        t.edit_rotation(1.0, 0.0, 0.0, 0.0);
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** edit_scale() always throws (not supported by REST API) */
static void test_twin_edit_scale_always_throws()
{
    Config cfg;
    cfg.api_key = "key"; // even with key, not supported
    Client c(cfg);
    Twin t(c, "twin-uuid", "Twin");
    bool threw = false;
    try
    {
        t.edit_scale(1.0, 1.0, 1.0);
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** twin uuid and name accessors */
static void test_twin_uuid_and_name()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    assert(t.uuid() == "uuid-abc");
    assert(t.name() == "My Twin");
}

/** twin environment_id settable */
static void test_twin_environment_id()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    assert(t.environment_id().empty());
    t.set_environment_id("env-123");
    assert(t.environment_id() == "env-123");
}

/** asset_id returns empty when no schema loaded */
static void test_twin_asset_id_empty_without_schema()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    assert(t.asset_id().empty());
}

/** capabilities_json returns "{}" when no schema loaded */
static void test_twin_capabilities_json_empty_without_schema()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    assert(t.capabilities_json() == "{}");
}

/** child_uuids returns empty vector when no schema loaded */
static void test_twin_child_uuids_empty_without_schema()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    assert(t.child_uuids().empty());
}

/** has_capability returns false when no schema loaded */
static void test_twin_has_capability_false_without_schema()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    assert(!t.has_capability("can_fly"));
    assert(!t.has_sensor());
    assert(!t.has_sensor("lidar"));
}

/** refresh() throws without api key */
static void test_twin_refresh_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    bool threw = false;
    try
    {
        t.refresh();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** delete_twin() throws without api key */
static void test_twin_delete_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    bool threw = false;
    try
    {
        t.delete_twin();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** get_joint_states() throws without api key */
static void test_twin_get_joint_states_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    bool threw = false;
    try
    {
        t.get_joint_states();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** get_schema() throws without api key */
static void test_twin_get_schema_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    bool threw = false;
    try
    {
        t.get_schema();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** subscribe() throws without MQTT client */
static void test_twin_subscribe_throws_without_mqtt()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    bool threw = false;
    try
    {
        t.subscribe([](const std::string&) {});
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** subscribe_position() throws without MQTT client */
static void test_twin_subscribe_position_throws_without_mqtt()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    bool threw = false;
    try
    {
        t.subscribe_position([](const std::string&) {});
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** subscribe_rotation() throws without MQTT client */
static void test_twin_subscribe_rotation_throws_without_mqtt()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    bool threw = false;
    try
    {
        t.subscribe_rotation([](const std::string&) {});
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** subscribe_joints() throws without MQTT client */
static void test_twin_subscribe_joints_throws_without_mqtt()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, "uuid-abc", "My Twin");
    bool threw = false;
    try
    {
        t.subscribe_joints([](const std::string&) {});
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** Twin schema-based constructor populates uuid/name from TwinSchema */
static void test_twin_schema_constructor()
{
    auto schema = std::make_shared<org::openapitools::client::model::TwinSchema>();
    schema->setUuid(utility::conversions::to_string_t("schema-uuid"));
    schema->setName(utility::conversions::to_string_t("Schema Twin"));
    schema->setFixedBase(true);
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(t.uuid() == "schema-uuid");
    assert(t.name() == "Schema Twin");
    assert(t.fixed_base());
}

/** Twin::parent() returns nullopt when no schema attached */
static void test_twin_parent_no_schema()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-uuid");
    auto p = t.parent();
    assert(!p.has_value());
}

/** Twin::parent() returns nullopt when schema has no attach_to_twin_uuid */
static void test_twin_parent_no_attach()
{
    auto schema = std::make_shared<org::openapitools::client::model::TwinSchema>();
    schema->setUuid(utility::conversions::to_string_t("t-uuid"));
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    auto p = t.parent();
    assert(!p.has_value());
}

/** Twin::children() returns empty when no schema or no child UUIDs */
static void test_twin_children_empty()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-uuid");
    auto children = t.children();
    assert(children.empty());
}

/** Twin::edit_rotation Euler overload compiles and runs without backend */
static void test_twin_edit_rotation_euler_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-uuid");
    bool threw = false;
    try
    {
        t.edit_rotation(90.0, 0.0, 0.0); // yaw=90°
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_twin_get_latest_frame_with_source_type_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-uuid");
    bool threw = false;
    try
    {
        t.get_latest_frame(false, "wrist_camera", "sim");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_twin_get_alerts_handle();
    test_twin_get_navigation_handle();
    test_twin_get_motion_handle();
    test_twin_get_joints_handle();
    test_twin_edit_position_throws_without_api_key();
    test_twin_edit_rotation_throws_without_api_key();
    test_twin_edit_scale_always_throws();
    test_twin_uuid_and_name();
    test_twin_environment_id();
    test_twin_asset_id_empty_without_schema();
    test_twin_capabilities_json_empty_without_schema();
    test_twin_child_uuids_empty_without_schema();
    test_twin_has_capability_false_without_schema();
    test_twin_refresh_throws_without_api_key();
    test_twin_delete_throws_without_api_key();
    test_twin_get_joint_states_throws_without_api_key();
    test_twin_get_schema_throws_without_api_key();
    test_twin_subscribe_throws_without_mqtt();
    test_twin_subscribe_position_throws_without_mqtt();
    test_twin_subscribe_rotation_throws_without_mqtt();
    test_twin_subscribe_joints_throws_without_mqtt();
    test_twin_schema_constructor();
    test_twin_parent_no_schema();
    test_twin_parent_no_attach();
    test_twin_children_empty();
    test_twin_edit_rotation_euler_throws_without_api_key();
    test_twin_get_latest_frame_with_source_type_throws_without_api_key();
    return 0;
}
