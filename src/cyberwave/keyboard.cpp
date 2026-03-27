#include "cyberwave/keyboard.h"
#include "cyberwave/exceptions.h"
#include "cyberwave/joints.h"
#include "cyberwave/twin.h"

#include <algorithm>
#include <cctype>

#if defined(__unix__) || defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#define CYBERWAVE_KEYBOARD_POSIX 1
#endif

namespace cyberwave
{

// --- KeyboardBindings ---

KeyboardBindings& KeyboardBindings::bind(const std::string& key, const std::string& joint_name,
                                         const std::string& direction)
{
    if (direction != "increase" && direction != "decrease")
        throw CyberwaveValidationError("direction must be 'increase' or 'decrease'");
    if (key.empty())
        throw CyberwaveValidationError("key is required");
    if (joint_name.empty())
        throw CyberwaveValidationError("joint_name is required");
    std::string key_upper = key;
    std::transform(key_upper.begin(), key_upper.end(), key_upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    bindings_.push_back({key_upper, joint_name, direction});
    return *this;
}

std::vector<KeyBinding> KeyboardBindings::build() const { return bindings_; }

// --- KeyboardTeleop ---

static std::string to_upper(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

KeyboardTeleop::KeyboardTeleop(Twin twin, const std::vector<KeyBinding>& bindings, double step, int rate_hz,
                               bool fetch_initial, bool verbose)
    : twin_(std::move(twin)), bindings_(bindings), step_(step), interval_seconds_(rate_hz > 0 ? 1.0 / rate_hz : 0.05),
      verbose_(verbose)
{
    if (fetch_initial)
    {
        try
        {
            positions_ = twin_.joints().get_all();
        }
        catch (...)
        {
            // ignore
        }
    }
}

void KeyboardTeleop::apply_binding(const KeyBinding& b)
{
    double delta = (b.direction == "increase") ? step_ : -step_;
    double current = 0.0;
    auto it = positions_.find(b.joint_name);
    if (it != positions_.end())
        current = it->second;
    double next_pos = current + delta;
    positions_[b.joint_name] = next_pos;
    try
    {
        twin_.joints().set(b.joint_name, next_pos, true);
        if (verbose_)
            (void)0; // optional: log to stderr
    }
    catch (...)
    {
    }
}

void KeyboardTeleop::apply_key(const std::string& key)
{
    std::string k = to_upper(key);
    for (const auto& b : bindings_)
    {
        if (b.key == k)
            apply_binding(b);
    }
}

void KeyboardTeleop::run(const std::string& stop_key)
{
    if (bindings_.empty())
        throw CyberwaveValidationError("No keyboard bindings configured");
    std::string stop = stop_key;
    if (!stop.empty())
        stop = to_upper(stop_key);

#if defined(CYBERWAVE_KEYBOARD_POSIX)
    int fd = STDIN_FILENO;
    struct termios old_tio;
    if (tcgetattr(fd, &old_tio) != 0)
        throw CyberwaveError("keyboard run: tcgetattr failed");
    struct termios tio = old_tio;
    tio.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));
    tcsetattr(fd, TCSANOW, &tio);

    while (true)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv;
        tv.tv_sec = static_cast<long>(interval_seconds_);
        tv.tv_usec = static_cast<long>((interval_seconds_ - static_cast<int>(interval_seconds_)) * 1000000);
        int r = select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (r > 0)
        {
            char c = 0;
            if (read(fd, &c, 1) == 1)
            {
                std::string key(1, static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                if (key == stop)
                    break;
                apply_key(key);
            }
        }
    }
    tcsetattr(fd, TCSADRAIN, &old_tio);
#else
    (void)stop;
    throw CyberwaveError("keyboard run() is only implemented on POSIX; use apply_key() with your own input loop");
#endif
}

// --- TwinControllerHandle ---

TwinControllerHandle::TwinControllerHandle(Twin twin) : twin_(std::move(twin)) {}

KeyboardTeleop TwinControllerHandle::keyboard(const std::vector<KeyBinding>& bindings, double step, int rate_hz,
                                              bool fetch_initial, bool verbose) const
{
    return KeyboardTeleop(twin_, bindings, step, rate_hz, fetch_initial, verbose);
}

} // namespace cyberwave
