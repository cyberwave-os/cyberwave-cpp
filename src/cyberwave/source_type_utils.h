#pragma once

#include "cyberwave/client.h"
#include "cyberwave/constants.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace cyberwave::detail
{

inline std::string normalize_source_type_token(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

inline std::string default_control_source_type(const Client& client)
{
    return client.config().runtime_mode == "simulation" ? SOURCE_TYPE_SIM_TELE : SOURCE_TYPE_TELE;
}

inline std::string normalize_control_source_type(const Client& client, const std::string& source_type)
{
    if (source_type.empty())
        return default_control_source_type(client);

    const std::string normalized = normalize_source_type_token(source_type);
    if (normalized == "sim" || normalized == "simulation")
        return SOURCE_TYPE_SIM_TELE;
    if (normalized == "live" || normalized == "real" || normalized == "real-world" || normalized == "teleoperation" ||
        normalized == SOURCE_TYPE_EDGE)
        return SOURCE_TYPE_TELE;
    return normalized;
}

inline std::string resolve_frame_source_type(const Client& client, const std::string& source_type)
{
    const std::string resolved = source_type.empty() ? client.config().source_type : source_type;
    const std::string normalized = normalize_source_type_token(resolved);
    if (normalized == "sim" || normalized == "simulation")
        return "sim";
    if (normalized == "tele" || normalized == "real" || normalized == "real-world" || normalized == "teleoperation")
        return "tele";
    return "";
}

} // namespace cyberwave::detail
