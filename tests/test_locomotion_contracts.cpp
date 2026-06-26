/**
 * Versioned locomotion control payload helpers.
 */
#include <cyberwave/locomotion_contracts.h>

#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>

using namespace cyberwave;

static void test_velocity_command_serializes_contract_payload()
{
    const auto command = build_locomotion_velocity_command(0.25, 0.0, 0.4, 750, "walk", "teleop");
    const std::string payload = command.to_json();

    assert(payload.find("\"contract\":\"locomotion.velocity_command.v1\"") != std::string::npos);
    assert(payload.find("\"linear_x\":0.25") != std::string::npos);
    assert(payload.find("\"linear_y\":0") != std::string::npos);
    assert(payload.find("\"angular_z\":0.4") != std::string::npos);
    assert(payload.find("\"duration_ms\":750") != std::string::npos);
    assert(payload.find("\"gait\":\"walk\"") != std::string::npos);
    assert(payload.find("\"origin\":\"teleop\"") != std::string::npos);
}

static void test_velocity_command_contains_schema_required_keys()
{
    const auto command = build_locomotion_velocity_command(0.25, 0.0, 0.4, 750, "walk", "teleop");
    const std::string payload = command.to_json();

    for (const char* key : LOCOMOTION_VELOCITY_COMMAND_REQUIRED_FIELDS)
    {
        assert(payload.find(std::string("\"") + key + "\"") != std::string::npos);
    }
}

static void test_velocity_command_rejects_non_contract_duration()
{
    bool threw = false;
    try
    {
        (void)build_locomotion_velocity_command(0.25, 0.0, 0.0, 1);
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_build_velocity_command_allows_schema_stop_duration()
{
    const auto command = build_locomotion_velocity_command(0.0, 0.0, 0.0, 0, "stand", "workflow");
    const std::string payload = command.to_json();

    assert(payload.find("\"duration_ms\":0") != std::string::npos);
    assert(payload.find("\"gait\":\"stand\"") != std::string::npos);
    assert(payload.find("\"origin\":\"workflow\"") != std::string::npos);
    assert(payload.find("\"contract\":\"locomotion.velocity_command.v1\"") != std::string::npos);
}

static void test_velocity_command_validator_accepts_stop_by_default()
{
    const auto command = stop_locomotion_velocity_command("workflow");

    validate_locomotion_velocity_command(command);
}

static void test_velocity_command_validator_can_reject_stop_when_required()
{
    bool threw = false;
    try
    {
        validate_locomotion_velocity_command(stop_locomotion_velocity_command(), false);
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_velocity_command_rejects_non_finite_velocities()
{
    bool threw = false;
    try
    {
        (void)build_locomotion_velocity_command(std::numeric_limits<double>::infinity(), 0.0, 0.0, 500);
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_stop_velocity_command_uses_canonical_stop_payload()
{
    const auto command = stop_locomotion_velocity_command("workflow");
    const std::string payload = command.to_json();

    assert(payload.find("\"duration_ms\":0") != std::string::npos);
    assert(payload.find("\"gait\":\"stand\"") != std::string::npos);
    assert(payload.find("\"origin\":\"workflow\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Contract literal tests — pin the values that must stay in sync with the
// backend schema (cyberwave-backend/src/app/contracts/
// locomotion.velocity_command.v1.schema.json).  Update here whenever the
// backend schema changes.
// ---------------------------------------------------------------------------

static void test_contract_identifier_value()
{
    assert(std::string(LOCOMOTION_VELOCITY_COMMAND_CONTRACT) == "locomotion.velocity_command.v1");
}

static void test_required_fields_set()
{
    // Mirrors schema["required"]: contract, linear_x, angular_z, duration_ms, gait, origin.
    const std::array<const char*, 6> expected = {"contract", "linear_x", "angular_z", "duration_ms", "gait", "origin"};
    assert(LOCOMOTION_VELOCITY_COMMAND_REQUIRED_FIELDS == expected);
}

static void test_valid_gaits()
{
    // Mirrors schema["properties"]["gait"]["enum"]: walk, trot, stand.
    assert(is_valid_locomotion_gait("walk"));
    assert(is_valid_locomotion_gait("trot"));
    assert(is_valid_locomotion_gait("stand"));
    assert(!is_valid_locomotion_gait("run"));
    assert(!is_valid_locomotion_gait(""));
}

static void test_valid_origins()
{
    // Mirrors schema["properties"]["origin"]["enum"]: teleop, ai_policy, navigation, workflow.
    assert(is_valid_locomotion_origin("teleop"));
    assert(is_valid_locomotion_origin("ai_policy"));
    assert(is_valid_locomotion_origin("navigation"));
    assert(is_valid_locomotion_origin("workflow"));
    assert(!is_valid_locomotion_origin("manual"));
    assert(!is_valid_locomotion_origin(""));
}

static void test_duration_ms_max_boundary()
{
    // schema["properties"]["duration_ms"]["maximum"] == 30000.
    (void)build_locomotion_velocity_command(0.0, 0.0, 0.0, 30000);

    bool threw = false;
    try
    {
        (void)build_locomotion_velocity_command(0.0, 0.0, 0.0, 30001);
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_velocity_command_serializes_contract_payload();
    test_velocity_command_contains_schema_required_keys();
    test_velocity_command_rejects_non_contract_duration();
    test_build_velocity_command_allows_schema_stop_duration();
    test_velocity_command_validator_accepts_stop_by_default();
    test_velocity_command_validator_can_reject_stop_when_required();
    test_velocity_command_rejects_non_finite_velocities();
    test_stop_velocity_command_uses_canonical_stop_payload();
    test_contract_identifier_value();
    test_required_fields_set();
    test_valid_gaits();
    test_valid_origins();
    test_duration_ms_max_boundary();
    return 0;
}
