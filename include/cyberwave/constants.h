/**
 * Cyberwave SDK — constants (aligned with Python constants.py).
 */

#ifndef CYBERWAVE_CONSTANTS_H
#define CYBERWAVE_CONSTANTS_H

#include <array>
#include <cstddef>
#include <string_view>

namespace cyberwave
{

/** Source type: edge leader. */
constexpr const char* SOURCE_TYPE_EDGE_LEADER = "edge_leader";
/** Source type: edge follower. */
constexpr const char* SOURCE_TYPE_EDGE_FOLLOWER = "edge_follower";
/** Source type: edge device. */
constexpr const char* SOURCE_TYPE_EDGE = "edge";
/** Source type: teleoperation. */
constexpr const char* SOURCE_TYPE_TELE = "tele";
/** Source type: editor. */
constexpr const char* SOURCE_TYPE_EDIT = "edit";
/** Source type: simulator. */
constexpr const char* SOURCE_TYPE_SIM = "sim";
/** Source type: simulated teleoperation commands. */
constexpr const char* SOURCE_TYPE_SIM_TELE = "sim_tele";

/** @brief All valid source type strings. */
inline constexpr std::array<const char*, 7> SOURCE_TYPES = {
    SOURCE_TYPE_EDGE_LEADER, SOURCE_TYPE_EDGE_FOLLOWER, SOURCE_TYPE_EDGE,     SOURCE_TYPE_TELE,
    SOURCE_TYPE_EDIT,        SOURCE_TYPE_SIM,           SOURCE_TYPE_SIM_TELE,
};

/** @brief Number of supported source types. */
constexpr std::size_t SOURCE_TYPES_SIZE = SOURCE_TYPES.size();

/**
 * @brief Return whether a string is a valid source type.
 * @param s Candidate source type string.
 * @return `true` when `s` matches one of `SOURCE_TYPES`.
 */
inline bool is_valid_source_type(std::string_view s)
{
    for (const char* t : SOURCE_TYPES)
    {
        if (s == t)
            return true;
    }
    return false;
}

} // namespace cyberwave

#endif // CYBERWAVE_CONSTANTS_H
