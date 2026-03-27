/**
 * Symmetric with Python: client.get_scene(environment_id), Scene get_twins, add_twin, dock, undock.
 */
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/scene.h>
#include <cyberwave/twin.h>

#include <cassert>
#include <string>

using namespace cyberwave;

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
    return 0;
}
