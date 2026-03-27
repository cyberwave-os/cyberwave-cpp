#include "cyberwave/utils.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace cyberwave
{

namespace
{

double wall_seconds()
{
    using Clock = std::chrono::system_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

double monotonic_seconds()
{
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

std::string get_hostname()
{
#if defined(_WIN32) || defined(_WIN64)
    char buf[256];
    DWORD len = static_cast<DWORD>(sizeof(buf));
    if (GetComputerNameA(buf, &len) != 0)
        return std::string(buf);
    return "unknown";
#else
    char buf[256];
    if (gethostname(buf, sizeof(buf)) == 0)
        return std::string(buf);
    return "unknown";
#endif
}

std::string get_platform()
{
#if defined(_WIN32) || defined(_WIN64)
    return "Windows-x64";
#else
    struct utsname u;
    if (uname(&u) == 0)
    {
        return std::string(u.sysname) + "-" + u.machine;
    }
    return "unknown";
#endif
}

std::string get_mac_placeholder() { return "00:00:00:00:00:00"; }

std::string hash_suffix(const std::string& raw)
{
    std::size_t h = std::hash<std::string>{}(raw);
    std::ostringstream os;
    os << std::hex << h;
    std::string s = os.str();
    if (s.size() > 12u)
        s = s.substr(0, 12);
    else if (s.size() < 12u)
        s = std::string(12 - s.size(), '0') + s;
    return s;
}

} // namespace

TimeReference::TimeReference()
{
    time_ = wall_seconds();
    time_monotonic_ = monotonic_seconds();
}

std::pair<double, double> TimeReference::update()
{
    std::lock_guard<std::mutex> lock(mutex_);
    time_ = wall_seconds();
    time_monotonic_ = monotonic_seconds();
    return {time_, time_monotonic_};
}

std::pair<double, double> TimeReference::read() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return {time_, time_monotonic_};
}

DeviceInfo get_device_info()
{
    DeviceInfo info;
    info.hostname = get_hostname();
    info.platform = get_platform();
    info.mac_address = get_mac_placeholder();
    return info;
}

std::string generate_fingerprint(const std::string& override)
{
    if (!override.empty())
        return override;
    const char* env = std::getenv("CYBERWAVE_EDGE_UUID");
    if (env && env[0] != '\0')
        return std::string(env);
    DeviceInfo info = get_device_info();
    std::string raw = info.hostname + "-" + info.mac_address + "-" + info.platform;
    std::string suffix = hash_suffix(raw);
    std::string prefix = info.hostname;
    if (prefix.size() > 15u)
        prefix = prefix.substr(0, 15);
    for (char& c : prefix)
    {
        if (c == ' ')
            c = '-';
        else if (c == '.')
            c = '-';
        else if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + 32);
    }
    while (!prefix.empty() && prefix.back() == '-')
        prefix.pop_back();
    return prefix + "-" + suffix;
}

std::string format_device_info_table(std::optional<DeviceInfo> info)
{
    DeviceInfo i = info.value_or(get_device_info());
    std::string fp = generate_fingerprint();
    std::string mac = i.mac_address;
    if (mac.size() > 8u)
        mac = mac.substr(0, 8) + ":xx:xx:xx";
    std::ostringstream os;
    os << "hostname:   " << i.hostname << "\n";
    os << "platform:   " << i.platform << "\n";
    os << "mac:        " << mac << "\n";
    os << "fingerprint: " << fp << "\n";
    return os.str();
}

} // namespace cyberwave
