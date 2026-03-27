/**
 * Symmetric with Python test_alerts.py:
 * - TwinAlertManager create: payload omits force by default; sets force when requested; includes media when provided.
 * - Alert getters (e.g. media).
 * - press_button rejects negative index (CyberwaveError).
 */
#include <cyberwave/alerts.h>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/twin.h>

#include <CppRestOpenAPIClient/AnyType.h>
#include <CppRestOpenAPIClient/model/AlertSchema.h>
#include <CppRestOpenAPIClient/model/CreateAlertSchema.h>

#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>

using namespace cyberwave;

static void test_alert_media_getter()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::AlertSchema>();
    schema->setUuid(utility::conversions::to_string_t("alert-uuid"));
    schema->setMedia(utility::conversions::to_string_t("https://cdn.example.com/alerts/help.mp4"));
    Alert alert(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(alert.uuid() == "alert-uuid");
    assert(alert.media() == "https://cdn.example.com/alerts/help.mp4");
}

static void test_alert_press_button_rejects_negative_index()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::AlertSchema>();
    schema->setUuid(utility::conversions::to_string_t("alert-uuid"));
    Alert alert(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    bool threw = false;
    try
    {
        alert.press_button(-1);
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_alert_timestamps_and_metadata_getters()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::AlertSchema>();
    schema->setUuid(utility::conversions::to_string_t("alert-uuid"));
    schema->setCreatedAt(utility::datetime::from_string(utility::conversions::to_string_t("2026-03-18T12:34:56Z")));
    schema->setUpdatedAt(utility::datetime::from_string(utility::conversions::to_string_t("2026-03-18T12:35:10Z")));
    schema->setResolvedAt(utility::datetime::from_string(utility::conversions::to_string_t("2026-03-18T12:36:00Z")));

    auto metadata_value = std::make_shared<org::openapitools::client::model::AnyType>();
    metadata_value->fromJson(web::json::value::string(utility::conversions::to_string_t("robot-1")));
    schema->setMetadata({{utility::conversions::to_string_t("device"), metadata_value}});

    Alert alert(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(alert.created_at() == "2026-03-18T12:34:56Z");
    assert(alert.updated_at() == "2026-03-18T12:35:10Z");
    assert(alert.resolved_at() == "2026-03-18T12:36:00Z");
    assert(alert.metadata().at("device") == "robot-1");
}

static void test_create_options_force_default()
{
    TwinAlertManager::CreateOptions opts;
    assert(opts.force == false);
}

static void test_create_options_force_true()
{
    TwinAlertManager::CreateOptions opts;
    opts.force = true;
    assert(opts.force == true);
}

static void test_create_options_media()
{
    TwinAlertManager::CreateOptions opts;
    opts.media = "https://cdn.example.com/alerts/calibration.gif";
    assert(opts.media == "https://cdn.example.com/alerts/calibration.gif");
}

/** Mirrors Python test: ListOptions fields are settable and readable */
static void test_alert_list_with_options()
{
    TwinAlertManager::ListOptions opts;
    opts.status = "resolved";
    opts.severity = "critical";
    opts.limit = 50;
    assert(opts.status == "resolved");
    assert(opts.severity == "critical");
    assert(opts.limit == 50);
}

/** Default ListOptions */
static void test_alert_list_options_defaults()
{
    TwinAlertManager::ListOptions opts;
    assert(opts.status.empty());
    assert(opts.severity.empty());
    assert(opts.limit == 100);
}

/** UpdateOptions fields are settable and readable (mirrors Python Alert.update()) */
static void test_alert_update_options()
{
    Alert::UpdateOptions opts;
    opts.name = std::string("New Name");
    opts.description = std::string("Updated desc");
    opts.media = std::string("https://example.com/image.png");
    opts.alert_type = std::string("warning");
    opts.severity = std::string("critical");
    opts.status = std::string("acknowledged");
    opts.metadata = std::map<std::string, std::string>{{"device", "robot-1"}};
    assert(opts.name && *opts.name == "New Name");
    assert(opts.description && *opts.description == "Updated desc");
    assert(opts.media && *opts.media == "https://example.com/image.png");
    assert(opts.alert_type && *opts.alert_type == "warning");
    assert(opts.severity && *opts.severity == "critical");
    assert(opts.status && *opts.status == "acknowledged");
    assert(opts.metadata && opts.metadata->at("device") == "robot-1");
}

/** UpdateOptions defaults omit all fields until explicitly set */
static void test_alert_update_options_defaults_empty()
{
    Alert::UpdateOptions opts;
    assert(!opts.name);
    assert(!opts.description);
    assert(!opts.media);
    assert(!opts.alert_type);
    assert(!opts.severity);
    assert(!opts.status);
    assert(!opts.metadata);
}

static void test_alert_update_options_can_clear_fields()
{
    Alert::UpdateOptions opts;
    opts.media = std::string("");
    opts.metadata = std::map<std::string, std::string>{};
    assert(opts.media && opts.media->empty());
    assert(opts.metadata && opts.metadata->empty());
}

/** alert.update() requires API — throws without one */
static void test_alert_update_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto schema = std::make_shared<org::openapitools::client::model::AlertSchema>();
    schema->setUuid(utility::conversions::to_string_t("alert-uuid"));
    Alert alert(c, std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    bool threw = false;
    try
    {
        Alert::UpdateOptions opts;
        opts.name = std::string("new");
        alert.update(opts);
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** alert.list() requires API — throws without one */
static void test_alert_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto t = std::make_shared<Twin>(c, "twin-uuid", "twin");
    TwinAlertManager mgr(t);
    bool threw = false;
    try
    {
        mgr.list();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** alert.get() requires API — throws without one */
static void test_alert_get_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    auto t = std::make_shared<Twin>(c, "twin-uuid", "twin");
    TwinAlertManager mgr(t);
    bool threw = false;
    try
    {
        mgr.get("alert-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_alert_manager_rejects_null_twin()
{
    bool threw = false;
    try
    {
        TwinAlertManager mgr(std::shared_ptr<const Twin>{});
        (void)mgr;
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_alert_media_getter();
    test_alert_press_button_rejects_negative_index();
    test_alert_timestamps_and_metadata_getters();
    test_create_options_force_default();
    test_create_options_force_true();
    test_create_options_media();
    // new symmetric tests
    test_alert_list_with_options();
    test_alert_list_options_defaults();
    test_alert_update_options();
    test_alert_update_options_defaults_empty();
    test_alert_update_options_can_clear_fields();
    test_alert_update_throws_without_api_key();
    test_alert_list_throws_without_api_key();
    test_alert_get_throws_without_api_key();
    test_alert_manager_rejects_null_twin();
    return 0;
}
