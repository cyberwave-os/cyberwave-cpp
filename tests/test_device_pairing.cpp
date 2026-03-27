/**
 * Device pairing (via twin metadata): list_devices, pair_device, unpair_device, get_device.
 * Without API key, list_devices throws.
 */
#include <cassert>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/device_pairing.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/twins.h>
#include <string>

using namespace cyberwave;

static void test_list_devices_requires_api()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    TwinManager tm(c);
    try
    {
        device_pairing::list_devices(tm, "any-twin-id");
        assert(false);
    }
    catch (const CyberwaveError&)
    {
    }
}

static void test_paired_device_struct()
{
    PairedDevice d;
    d.uuid = "u";
    d.fingerprint = "fp";
    d.twin_uuid = "twin";
    assert(d.uuid == "u");
    assert(d.fingerprint == "fp");
    assert(d.twin_uuid == "twin");
}

int main()
{
    test_list_devices_requires_api();
    test_paired_device_struct();
    return 0;
}
