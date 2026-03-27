/**
 * Navigation handle and NavigationPlan: plan builder, waypoints, to_mission; goto/stop (no live backend).
 */
#include <cassert>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/navigation.h>
#include <cyberwave/twin.h>
#include <string>
#include <vector>

using namespace cyberwave;

static void test_navigation_handle_twin_uuid()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    TwinNavigationHandle n = t.navigation();
    assert(n.twin_uuid() == "twin-123");
}

static void test_navigation_plan_builder()
{
    NavigationPlan p("my-plan");
    p.waypoint(1.0, 0.0, 0.0).waypoint(2.0, 0.0, 1.0);
    std::vector<std::vector<double>> w = p.build_waypoints();
    assert(w.size() == 2);
    assert(w[0].size() == 4);
    assert(w[0][0] == 1.0 && w[0][1] == 0.0 && w[0][2] == 0.0);
    assert(w[1][0] == 2.0 && w[1][2] == 1.0);
}

static void test_navigation_plan_to_mission()
{
    NavigationPlan p("plan-id");
    p.waypoint(0, 0, 0).waypoint(1, 1, 0);
    auto mission = p.to_mission("twin-uuid", "my-mission");
    assert(mission.id == "plan-id");
    assert(mission.name == "my-mission");
    assert(mission.twin_uuid == "twin-uuid");
    assert(mission.waypoints.size() == 2);
}

static void test_navigation_handle_plan()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    TwinNavigationHandle n = t.navigation();
    NavigationPlan p1 = n.plan();
    assert(p1.plan_id() == "plan");
    NavigationPlan p2 = n.plan("custom-id");
    assert(p2.plan_id() == "custom-id");
    p2.waypoint(0, 0, 0);
    assert(p2.build_waypoints().size() == 1);
}

static void test_use_controller_clear_controller()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    TwinNavigationHandle n = t.navigation();
    n.use_controller("policy-uuid");
    NavigationPlan p = n.plan("p");
    auto mission = p.to_mission("twin-123", "m");
    n.clear_controller();
}

static void test_goto_stop_no_backend()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    t.navigation().goto_position(0, 0, 0);
    t.navigation().goto_position(1, 2, 0, 1.57, "env-1", "edge", {1.0, 0.0, 0.0, 0.0}, "{\"type\":\"any\"}",
                                 "{\"priority\":1}");
    t.navigation().stop();
    t.navigation().pause();
    t.navigation().resume();
    t.navigation().follow_path({{0, 0, 0}, {1, 0, 0}});
    t.navigation().follow_path({{0, 0, 0}}, 2.0, 3, "env-1", "edge", "{}", "{\"loop\":true}");
}

/** NavigationPlan::build() returns valid JSON */
static void test_navigation_plan_build()
{
    NavigationPlan p("plan-1");
    p.set_name("Test Plan").waypoint(1.0, 2.0, 0.0, 0.5, "wp-1", {1.0, 0.0, 0.0, 0.0}, "{\"key\":1}");
    std::string json = p.build();
    assert(!json.empty());
    assert(json.find("plan-1") != std::string::npos);
    assert(json.find("Test Plan") != std::string::npos);
    assert(json.find("waypoints") != std::string::npos);
}

/** Rich waypoint overload stores id, rotation, metadata */
static void test_rich_waypoint()
{
    NavigationPlan p;
    p.waypoint(1.0, 2.0, 3.0, 0.0, "my-id", {1.0, 0.0, 0.0, 0.0}, "{\"meta\":true}");
    auto wps = p.build_waypoints();
    assert(wps.size() == 1);
    assert(wps[0][0] == 1.0);
    // build() JSON should contain id
    std::string json = p.build();
    assert(json.find("my-id") != std::string::npos);
}

/** Mirrors Python NavigationPlan.set_name() */
static void test_navigation_plan_set_name()
{
    NavigationPlan p("plan-id");
    assert(p.name() == "plan-id"); // default name == plan_id
    p.set_name("custom-name");
    assert(p.name() == "custom-name");
    // set_name returns *this for chaining
    NavigationPlan p2;
    p2.set_name("chained").waypoint(0, 0, 0);
    assert(p2.name() == "chained");
    assert(p2.build_waypoints().size() == 1);
}

/** Mirrors Python NavigationPlan.with_controller() */
static void test_navigation_plan_with_controller()
{
    NavigationPlan p;
    assert(p.controller_policy_uuid().empty());
    p.with_controller("policy-uuid-123");
    assert(p.controller_policy_uuid() == "policy-uuid-123");
    // Wired into to_mission()
    auto m = p.to_mission("twin-uuid");
    assert(m.controller_policy_uuid == "policy-uuid-123");
}

/** Mirrors Python NavigationPlan.set_metadata() */
static void test_navigation_plan_set_metadata()
{
    NavigationPlan p;
    assert(p.metadata().empty());
    p.set_metadata({{"priority", "high"}, {"zone", "A"}});
    assert(p.metadata().size() == 2);
    assert(p.metadata().at("priority") == "high");
    assert(p.metadata().at("zone") == "A");
    // Wired into to_mission()
    auto m = p.to_mission("t");
    assert(m.metadata.at("priority") == "high");
}

/** Mirrors Python NavigationPlan.extend() */
static void test_navigation_plan_extend()
{
    NavigationPlan p;
    p.waypoint(0, 0, 0);
    // extend with [x,y,z] entries
    p.extend({{1.0, 0.0, 0.0}, {2.0, 0.0, 0.5, 0.1}});
    auto w = p.build_waypoints();
    assert(w.size() == 3);
    assert(w[1][0] == 1.0);
    assert(w[2][3] == 0.1); // yaw preserved
    // extend returns *this for chaining
    NavigationPlan p2;
    p2.extend({{0, 0, 0}}).extend({{1, 1, 0}});
    assert(p2.build_waypoints().size() == 2);
}

/** Full chain: set_name + with_controller + set_metadata + waypoint + extend */
static void test_navigation_plan_full_chain()
{
    NavigationPlan p("mission-1");
    p.set_name("Patrol")
        .with_controller("ctrl-abc")
        .set_metadata({{"loop", "true"}})
        .waypoint(0, 0, 0)
        .extend({{1, 0, 0}, {2, 0, 0}});
    assert(p.name() == "Patrol");
    assert(p.controller_policy_uuid() == "ctrl-abc");
    assert(p.metadata().at("loop") == "true");
    assert(p.build_waypoints().size() == 3);
    auto m = p.to_mission("twin-uuid", "Override Name");
    assert(m.name == "Override Name");
    assert(m.controller_policy_uuid == "ctrl-abc");
    assert(m.metadata.at("loop") == "true");
}

/** to_mission with rich params: mission_id, description, created_at, is_active */
static void test_navigation_plan_to_mission_rich()
{
    NavigationPlan p("plan-1");
    p.set_name("Patrol").waypoint(0, 0, 0);
    auto m = p.to_mission("twin-uuid", "MyMission", "mission-abc", "A description", "2026-01-01T00:00:00", true);
    assert(m.id == "mission-abc");
    assert(m.name == "MyMission");
    assert(m.description == "A description");
    assert(m.twin_uuid == "twin-uuid");
    assert(m.created_at == "2026-01-01T00:00:00");
    assert(m.is_active.has_value());
    assert(*m.is_active == true);
}

/** to_mission defaults: id from plan_id, created_at auto-filled */
static void test_navigation_plan_to_mission_defaults()
{
    NavigationPlan p("my-plan");
    auto m = p.to_mission("t");
    assert(m.id == "my-plan");
    assert(!m.created_at.empty());    // auto-generated
    assert(!m.is_active.has_value()); // optional not set
}

/** stop/pause/resume accept environment_uuid + source_type (no-op without backend) */
static void test_stop_pause_resume_kwargs()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-nav");
    TwinNavigationHandle n = t.navigation();
    // All no-ops without backend, should not throw
    n.stop("env-1", "tele");
    n.pause("env-1");
    n.resume("", "sim");
    n.stop();
    n.pause();
    n.resume();
}

int main()
{
    test_navigation_handle_twin_uuid();
    test_navigation_plan_builder();
    test_navigation_plan_to_mission();
    test_navigation_handle_plan();
    test_use_controller_clear_controller();
    test_goto_stop_no_backend();
    test_navigation_plan_build();
    test_rich_waypoint();
    // new symmetric tests
    test_navigation_plan_set_name();
    test_navigation_plan_with_controller();
    test_navigation_plan_set_metadata();
    test_navigation_plan_extend();
    test_navigation_plan_full_chain();
    test_navigation_plan_to_mission_rich();
    test_navigation_plan_to_mission_defaults();
    test_stop_pause_resume_kwargs();
    return 0;
}