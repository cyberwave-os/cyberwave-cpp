/**
 * Tests for utils and constants (aligned with Python test_imports.py utils/constants checks).
 */

#include "cyberwave/constants.h"
#include "cyberwave/utils.h"

#include <cassert>
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

    assert(cyberwave::SOURCE_TYPES_SIZE == 6);
    bool has_edge = false;
    for (const char* t : cyberwave::SOURCE_TYPES)
    {
        if (std::string(t) == "edge")
            has_edge = true;
    }
    assert(has_edge);
    assert(cyberwave::is_valid_source_type("edge"));
    assert(cyberwave::is_valid_source_type("tele"));
    assert(!cyberwave::is_valid_source_type("invalid"));

    // --- TimeReference ---
    cyberwave::TimeReference tr;
    assert(tr.read().first >= 0);
    assert(tr.read().second >= 0);
    auto uv = tr.update();
    assert(uv.first >= 0);
    assert(uv.second >= 0);

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
