#include "cyberwave/utils.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
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

// Acquisition needs a clock that survives suspend, or the (wall, monotonic) offset
// breaks by the whole sleep. On Linux steady_clock is CLOCK_MONOTONIC, which stops,
// so prefer CLOCK_BOOTTIME; on Darwin steady_clock is already mach_continuous_time().
// Probed once rather than assumed, because the macro is visible if the *build* host's
// headers had the clock, which does not promise the running kernel does — the name and
// the flag then describe the clock actually being read.
// See "Acquisition stamps come from a suspend-inclusive clock" in cyberwave-sdks/README.md.
struct MonotonicClock
{
    const char* name;
    bool suspend_inclusive;
};

const MonotonicClock& selected_monotonic_clock()
{
    static const MonotonicClock selected = []
    {
#if defined(CLOCK_BOOTTIME)
        struct timespec probe;
        if (clock_gettime(CLOCK_BOOTTIME, &probe) == 0)
            return MonotonicClock{"CLOCK_BOOTTIME", true};
#endif
#if defined(__APPLE__)
        return MonotonicClock{"mach_continuous_time", true};
#else
        return MonotonicClock{"steady_clock", false};
#endif
    }();
    return selected;
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

const char* monotonic_clock_name() { return selected_monotonic_clock().name; }

bool monotonic_clock_is_suspend_inclusive() { return selected_monotonic_clock().suspend_inclusive; }

double monotonic_seconds()
{
#if defined(CLOCK_BOOTTIME)
    if (selected_monotonic_clock().suspend_inclusive)
    {
        struct timespec ts;
        if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0)
            return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
    }
#endif
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

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
