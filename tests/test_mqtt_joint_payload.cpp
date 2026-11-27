/**
 * Wire shape of CyberwaveMQTTClient::update_joints_state(), checked without a
 * broker through detail::build_joints_state_message(), which is exactly what
 * that method publishes on cyberwave/joint/{twin_uuid}/update.
 *
 * The joint bus carries four independent channels -- positions, velocities,
 * accelerations, efforts. An empty map means "not measured": its key is
 * omitted, never zero-filled, and effort is never aliased to acceleration.
 * The same holds for targets (detail::build_joint_targets_message()), which
 * carry each channel under its `target_` key.
 */

#include "mqtt_client.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

using namespace cyberwave;

namespace
{

// Release builds define NDEBUG, which compiles <cassert> away; this does not.
void check(bool ok, const char* what)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::abort();
    }
}

using Channel = std::map<std::string, double>;

const Channel kPositions = {{"j1", 1.0}, {"j2", -0.5}};

void test_positions_only_stays_flat()
{
    const json m = detail::build_joints_state_message("edge", kPositions, {}, {}, -1.0, "", "", "");
    check(m == json{{"source_type", "edge"}, {"j1", 1.0}, {"j2", -0.5}}, "positions-only payload is flat");
    check(!m.contains("accelerations"), "flat payload has no accelerations");
}

void test_accelerations_alone_trigger_the_aggregated_format()
{
    const Channel acc = {{"j1", 0.25}, {"j2", 0.0}};
    const json m = detail::build_joints_state_message("edge", kPositions, {}, {}, -1.0, "", "", "", acc);
    check(m == json{{"source_type", "edge"},
                    {"positions", {{"j1", 1.0}, {"j2", -0.5}}},
                    {"accelerations", {{"j1", 0.25}, {"j2", 0.0}}}},
          "accelerations published as their own aggregated channel");
    // An explicit 0.0 is a real measurement and survives.
    check(m["accelerations"]["j2"].get<double>() == 0.0, "explicit zero acceleration kept");
    check(!m.contains("efforts"), "no efforts fabricated from accelerations");
    check(!m.contains("velocities"), "no velocities fabricated");
    check(!m.contains("target_accelerations"), "measured state is never a target");
}

void test_efforts_are_never_aliased_to_accelerations()
{
    const Channel eff = {{"j1", 0.1}};
    const json m = detail::build_joints_state_message("edge", kPositions, {}, eff, -1.0, "", "", "");
    check(m.contains("efforts") && m["efforts"] == json{{"j1", 0.1}}, "efforts published");
    check(!m.contains("accelerations"), "efforts never stand in for accelerations");
}

void test_every_channel_and_metadata()
{
    const Channel vel = {{"j1", 0.5}};
    const Channel acc = {{"j1", 2.0}};
    const Channel eff = {{"j1", 0.1}};
    const json m = detail::build_joints_state_message("sim_tele", kPositions, vel, eff, 1234.5, "wl-uuid", "sess-1",
                                                      "openvla", acc);
    check(m == json{{"source_type", "sim_tele"},
                    {"positions", {{"j1", 1.0}, {"j2", -0.5}}},
                    {"velocities", {{"j1", 0.5}}},
                    {"accelerations", {{"j1", 2.0}}},
                    {"efforts", {{"j1", 0.1}}},
                    {"timestamp", 1234.5},
                    {"workload_uuid", "wl-uuid"},
                    {"session_id", "sess-1"},
                    {"source_subtype", "openvla"}},
          "full aggregated payload");
}

void test_single_joint_acceleration_is_appended_last()
{
    // `acceleration` was added after `effort`, so a positional aggregate
    // written before it still means what it did. That pre-acceleration
    // initialiser is the point here, so -Wmissing-field-initializers is too.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
    const JointState legacy{1.0, 2.0, 3.0};
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    check(legacy.effort == 3.0, "third initialiser is still effort");
    check(!legacy.acceleration.has_value(), "acceleration absent unless given");
}

// --- Targets: CyberwaveMQTTClient::update_joint_targets() publishes exactly
// detail::build_joint_targets_message(), the Python SDK's
// update_joints_state(..., as_targets=True).

void test_acceleration_only_targets()
{
    const Channel acc = {{"j1", 1.5}, {"j2", 0.0}};
    const json m = detail::build_joint_targets_message("sim_tele", {}, {}, {}, 1234.5, "", "", "", acc);
    check(m == json{{"source_type", "sim_tele"},
                    {"target_accelerations", {{"j1", 1.5}, {"j2", 0.0}}},
                    {"timestamp", 1234.5}},
          "acceleration alone is a complete target");
    check(m["target_accelerations"]["j2"].get<double>() == 0.0, "explicit zero acceleration target kept");
    for (const char* absent :
         {"target_positions", "target_velocities", "target_efforts", "positions", "accelerations", "efforts"})
        check(!m.contains(absent), "no other channel fabricated for an acceleration target");
}

void test_effort_targets_are_never_aliased_to_accelerations()
{
    const json m = detail::build_joint_targets_message("sim_tele", {}, {}, {{"j1", 0.4}}, 1.0, "", "", "");
    check(m.contains("target_efforts") && m["target_efforts"] == json{{"j1", 0.4}}, "effort target published");
    check(!m.contains("target_accelerations"), "effort targets never stand in for accelerations");
    check(!m.contains("efforts"), "an effort target is never measured effort");
}

void test_position_targets_never_read_as_measured_state()
{
    const json m = detail::build_joint_targets_message("sim_tele", kPositions, {}, {}, 1.0, "", "", "");
    check(m == json{{"source_type", "sim_tele"}, {"target_positions", {{"j1", 1.0}, {"j2", -0.5}}}, {"timestamp", 1.0}},
          "position-only target stays aggregated under target_positions");
    check(!m.contains("j1"), "a target is never the flat measured format");
}

void test_every_target_channel_and_metadata()
{
    const json m = detail::build_joint_targets_message("sim_tele", kPositions, {{"j1", 0.5}}, {{"j1", 0.1}}, 1234.5,
                                                       "wl-uuid", "sess-1", "openvla", {{"j1", 2.0}});
    check(m == json{{"source_type", "sim_tele"},
                    {"target_positions", {{"j1", 1.0}, {"j2", -0.5}}},
                    {"target_velocities", {{"j1", 0.5}}},
                    {"target_accelerations", {{"j1", 2.0}}},
                    {"target_efforts", {{"j1", 0.1}}},
                    {"timestamp", 1234.5},
                    {"workload_uuid", "wl-uuid"},
                    {"session_id", "sess-1"},
                    {"source_subtype", "openvla"}},
          "full target payload");
}

} // namespace

int main()
{
    test_positions_only_stays_flat();
    test_accelerations_alone_trigger_the_aggregated_format();
    test_efforts_are_never_aliased_to_accelerations();
    test_every_channel_and_metadata();
    test_single_joint_acceleration_is_appended_last();
    test_acceleration_only_targets();
    test_effort_targets_are_never_aliased_to_accelerations();
    test_position_targets_never_read_as_measured_state();
    test_every_target_channel_and_metadata();
    std::puts("test_mqtt_joint_payload: OK");
    return 0;
}
