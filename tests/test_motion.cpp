/**
 * Motion handle: construct from twin, list_keyframes, pose, animation, plan (no live backend).
 */
#include <cassert>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/motion.h>
#include <cyberwave/twin.h>
#include <string>
#include <vector>

using namespace cyberwave;

static void test_motion_handle_twin_uuid()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    TwinMotionHandle m = t.motion();
    assert(m.twin_uuid() == "twin-123");
}

static void test_list_keyframes_no_backend()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    std::vector<std::string> kf = t.motion().list_keyframes();
    assert(kf.empty());
    kf = t.motion().list_keyframes("env-456");
    assert(kf.empty());
}

static void test_pose_animation_plan_no_backend()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    TwinMotionHandle m = t.motion();
    m.pose("stand");
    m.pose("stand", "env-1", false, false, "edge", 200, 500);
    m.animation("walk");
    m.animation("wave", "env-1", true, false, "tele", 100, -1);
    m.plan("{}");
    m.plan("{}", "env-1", false, false, "edge", 100);
    m.plan_legacy("motion_plan", "{}");
}

/** ScopedMotionHandle via in_environment() */
static void test_scoped_motion_handle()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    auto scoped = t.motion().in_environment("env-xyz");
    // With no backend, these are no-ops
    scoped.pose("stand");
    scoped.animation("wave");
    auto kf = scoped.list_keyframes();
    assert(kf.empty());
    auto anims = scoped.list_animations();
    assert(anims.empty());
}

/** list_animations returns empty when no backend */
static void test_list_animations_no_backend()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    std::vector<std::string> anims = t.motion().list_animations();
    assert(anims.empty());
    anims = t.motion().list_animations("env-456");
    assert(anims.empty());
}

/** ScopedMotionHandle::plan() delegates to parent with env_uuid */
static void test_scoped_plan_no_backend()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-scoped");
    auto scoped = t.motion().in_environment("env-abc");
    scoped.plan("{}"); // no-op without backend, should not throw
    scoped.plan("{}", true, false, "tele", 100);
}

/** TwinMotionHandle::environment/twin/asset return correct scope */
static void test_motion_scope_handles()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-scope");
    auto env_scope = t.motion().environment();
    assert(env_scope.scope() == "environment");
    assert(env_scope.environment_uuid().empty());

    auto twin_scope = t.motion().twin();
    assert(twin_scope.scope() == "twin");

    auto asset_scope = t.motion().asset();
    assert(asset_scope.scope() == "asset");

    // All no-ops without backend
    env_scope.pose("a");
    twin_scope.animation("wave");
    asset_scope.plan("{}");
    auto kf = asset_scope.list_keyframes();
    assert(kf.empty());
}

/** in_environment() still works and scope is "environment" */
static void test_in_environment_scope()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-env");
    auto scoped = t.motion().in_environment("env-xyz");
    assert(scoped.scope() == "environment");
    assert(scoped.environment_uuid() == "env-xyz");
}

int main()
{
    test_motion_handle_twin_uuid();
    test_list_keyframes_no_backend();
    test_pose_animation_plan_no_backend();
    test_scoped_motion_handle();
    test_list_animations_no_backend();
    test_scoped_plan_no_backend();
    test_motion_scope_handles();
    test_in_environment_scope();
    return 0;
}
