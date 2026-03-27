/**
 * Symmetric with Python: client.assets, AssetManager, Asset view.
 */
#include <cyberwave/assets.h>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>

#include <CppRestOpenAPIClient/model/AssetListSchema.h>
#include <CppRestOpenAPIClient/model/AssetSchema.h>

#include <cassert>
#include <string>

using namespace cyberwave;

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
    Asset asset = Asset::from_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(asset.uuid() == "asset-uuid");
    assert(asset.name() == "My Asset");
    assert(asset.description() == "Description");
}

static void test_asset_view_from_asset_list_schema()
{
    auto schema = std::make_shared<org::openapitools::client::model::AssetListSchema>();
    schema->setUuid(utility::conversions::to_string_t("list-uuid"));
    schema->setName(utility::conversions::to_string_t("List Asset"));
    schema->setDescription(utility::conversions::to_string_t("List desc"));
    Asset asset = Asset::from_list_schema(std::shared_ptr<void>(std::static_pointer_cast<void>(schema)));
    assert(asset.uuid() == "list-uuid");
    assert(asset.name() == "List Asset");
    assert(asset.description() == "List desc");
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

int main()
{
    test_client_has_assets();
    test_asset_view_from_asset_schema();
    test_asset_view_from_asset_list_schema();
    test_assets_list_throws_without_api_key();
    test_assets_get_by_registry_id_throws_without_api_key();
    test_assets_get_universal_schema_throws_without_api_key();
    test_assets_patch_universal_schema_throws_without_api_key();
    return 0;
}
