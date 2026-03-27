/**
 * @brief Device pairing helpers built on top of twin metadata.
 */

#ifndef CYBERWAVE_DEVICE_PAIRING_H
#define CYBERWAVE_DEVICE_PAIRING_H

#include "cyberwave/twins.h"

#include <map>
#include <string>
#include <vector>

namespace cyberwave
{

/** @brief One edge device paired to a twin. */
struct PairedDevice
{
    std::string uuid; /**< Deterministic UUID from twin_id + fingerprint (UUID5). */
    std::string fingerprint;
    std::string twin_uuid;
    std::string hostname;
    std::string platform;
    std::string status; /**< "online" or "offline". */
    std::string last_heartbeat;
    std::string last_ip_address;
    std::string paired_at;
    std::string updated_at;

    /** Optional edge/camera config (keys depend on backend). */
    std::map<std::string, std::string> edge_config;
};

/** @brief Device pairing operations layered on top of `TwinManager`. */
namespace device_pairing
{

/**
 * Pair an edge device to a twin (persists in twin metadata).
 * Fingerprint must be unique across twins; use generate_fingerprint() for a stable device id.
 */
PairedDevice pair_device(const TwinManager& twins, const std::string& twin_id, const std::string& fingerprint,
                         const std::string& hostname = "", const std::string& platform = "",
                         const std::map<std::string, std::string>& edge_config = {});

/** List all edge devices paired to this twin. */
std::vector<PairedDevice> list_devices(const TwinManager& twins, const std::string& twin_id);

/** Get one paired device by its deterministic uuid (from list_devices). */
PairedDevice get_device(const TwinManager& twins, const std::string& twin_id, const std::string& device_uuid);

/** Unpair (remove) an edge device from this twin. Returns message. */
std::string unpair_device(const TwinManager& twins, const std::string& twin_id, const std::string& device_uuid);

} // namespace device_pairing

} // namespace cyberwave

#endif // CYBERWAVE_DEVICE_PAIRING_H
