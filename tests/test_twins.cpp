/**
 * Symmetric with Python test_edges style: client.twins, TwinManager.
 */
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/twin.h>
#include <cyberwave/twins.h>

#include <cassert>
#include <string>

using namespace cyberwave;

static void test_client_has_twins()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager t = c.twins();
    (void)t;
    assert(true);
}

static void test_twins_list_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager t = c.twins();
    bool threw = false;
    try
    {
        t.list();
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_twins_get_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager t = c.twins();
    bool threw = false;
    try
    {
        t.get("some-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_twins_get_joint_states_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager t = c.twins();
    bool threw = false;
    try
    {
        t.get_joint_states("some-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_twins_get_latest_frame_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager t = c.twins();
    bool threw = false;
    try
    {
        t.get_latest_frame("some-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_twins_get_calibration_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager t = c.twins();
    bool threw = false;
    try
    {
        t.get_calibration("some-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

static void test_twins_get_universal_schema_throws_without_api_key()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager t = c.twins();
    bool threw = false;
    try
    {
        t.get_universal_schema_at_path("some-uuid");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** get_latest_frame with sensor_id (throws without API key) */
static void test_twins_get_latest_frame_with_sensor_id_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager t = c.twins();
    bool threw = false;
    try
    {
        t.get_latest_frame("some-uuid", false, "wrist_camera");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

/** get_calibration with robot_type (throws without API key) */
static void test_twins_get_calibration_with_robot_type_throws()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager t = c.twins();
    bool threw = false;
    try
    {
        t.get_calibration("some-uuid", "amr");
    }
    catch (const CyberwaveError&)
    {
        threw = true;
    }
    assert(threw);
}

int main()
{
    test_client_has_twins();
    test_twins_list_throws_without_api_key();
    test_twins_get_throws_without_api_key();
    test_twins_get_joint_states_throws_without_api_key();
    test_twins_get_latest_frame_throws_without_api_key();
    test_twins_get_calibration_throws_without_api_key();
    test_twins_get_universal_schema_throws_without_api_key();
    test_twins_get_latest_frame_with_sensor_id_throws();
    test_twins_get_calibration_with_robot_type_throws();
    return 0;
}
