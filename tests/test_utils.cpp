/**
 * Tests for utils and constants (aligned with Python test_imports.py utils/constants checks).
 */

#include "cyberwave/constants.h"
#include "cyberwave/utils.h"

#include <cassert>
#include <cmath>
#include <ctime>
#include <string>

int main()
{
    // --- Constants ---
    assert(cyberwave::SOURCE_TYPE_EDGE != nullptr);
    assert(std::string(cyberwave::SOURCE_TYPE_EDGE) == "edge");
    assert(std::string(cyberwave::SOURCE_TYPE_TELE) == "tele");
    assert(std::string(cyberwave::SOURCE_TYPE_EDIT) == "edit");
    assert(std::string(cyberwave::SOURCE_TYPE_SIM) == "sim");
    assert(std::string(cyberwave::SOURCE_TYPE_EDGE_LEADER) == "edge_leader");
    assert(std::string(cyberwave::SOURCE_TYPE_EDGE_FOLLOWER) == "edge_follower");

    assert(cyberwave::SOURCE_TYPES_SIZE == 7);
    bool has_edge = false;
    for (const char* t : cyberwave::SOURCE_TYPES)
    {
        if (std::string(t) == "edge")
            has_edge = true;
    }
    assert(has_edge);
    assert(cyberwave::is_valid_source_type("edge"));
    assert(cyberwave::is_valid_source_type("tele"));
    assert(cyberwave::is_valid_source_type("sim_tele"));
    assert(!cyberwave::is_valid_source_type("invalid"));

    // --- TimeReference ---
    cyberwave::TimeReference tr;
    assert(tr.read().first >= 0);
    assert(tr.read().second >= 0);
    auto uv = tr.update();
    assert(uv.first >= 0);
    assert(uv.second >= 0);

    // Both platforms the SDK ships on have a suspend-inclusive clock, so falling
    // back is never the right answer here.
    assert(cyberwave::monotonic_clock_is_suspend_inclusive());

    // monotonic_seconds() is what CameraStreamer::stream_loop reads for the SEI
    // capture stamp, so pin it directly and not only through TimeReference.
    const double acquisition = cyberwave::monotonic_seconds();
    assert(acquisition > 0);
    assert(std::fabs(tr.update().second - acquisition) < 0.5);
#if defined(CLOCK_BOOTTIME)
    assert(std::string(cyberwave::monotonic_clock_name()) == "CLOCK_BOOTTIME");
    struct timespec bt;
    const int bt_rc = clock_gettime(CLOCK_BOOTTIME, &bt);
    (void)bt_rc;
    assert(bt_rc == 0);
    const double boottime = static_cast<double>(bt.tv_sec) + static_cast<double>(bt.tv_nsec) * 1e-9;
    assert(std::fabs(acquisition - boottime) < 0.5);
#elif defined(__APPLE__)
    assert(std::string(cyberwave::monotonic_clock_name()) == "mach_continuous_time");
#endif

    // --- Device info and fingerprint ---
    cyberwave::DeviceInfo info = cyberwave::get_device_info();
    assert(!info.hostname.empty() || info.hostname == "unknown");
    assert(!info.platform.empty());
    assert(!info.mac_address.empty());

    std::string fp = cyberwave::generate_fingerprint();
    assert(!fp.empty());

    std::string fp_override = cyberwave::generate_fingerprint("my-custom-id");
    assert(fp_override == "my-custom-id");

    std::string table = cyberwave::format_device_info_table();
    assert(table.find("hostname") != std::string::npos);
    assert(table.find("fingerprint") != std::string::npos);

    return 0;
}
