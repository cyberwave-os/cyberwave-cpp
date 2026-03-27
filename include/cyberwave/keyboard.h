/**
 * @brief Keyboard teleoperation helpers for joint-based twin control.
 */

#ifndef CYBERWAVE_KEYBOARD_H
#define CYBERWAVE_KEYBOARD_H

#include "cyberwave/twin.h"

#include <map>
#include <string>
#include <vector>

namespace cyberwave
{

/** @brief Single keyboard binding mapping a key to a joint direction. */
struct KeyBinding
{
    std::string key;
    std::string joint_name;
    std::string direction; // "increase" or "decrease"
};

/** @brief Builder for keyboard teleoperation bindings. */
class KeyboardBindings
{
public:
    KeyboardBindings() = default;

    /** Bind a key to a joint action. Returns *this for chaining. */
    KeyboardBindings& bind(const std::string& key, const std::string& joint_name,
                           const std::string& direction = "increase");

    /** Return the bindings as a vector. */
    std::vector<KeyBinding> build() const;

private:
    std::vector<KeyBinding> bindings_;
};

/**
 * @brief Terminal keyboard teleop loop that updates twin joints.
 */
class KeyboardTeleop
{
public:
    /** step in degrees per keypress; rate_hz polling; fetch_initial loads current positions. */
    KeyboardTeleop(Twin twin, const std::vector<KeyBinding>& bindings, double step = 0.05, int rate_hz = 20,
                   bool fetch_initial = true, bool verbose = true);

    /** Run the teleop loop until stop_key is pressed. POSIX (termios/select) only. */
    void run(const std::string& stop_key = "q");

    /** Apply one key: update internal state and call twin->joints().set(...). */
    void apply_key(const std::string& key);

private:
    Twin twin_;
    std::vector<KeyBinding> bindings_;
    double step_;
    double interval_seconds_;
    bool verbose_;
    std::map<std::string, double> positions_;

    void apply_binding(const KeyBinding& b);
};

/** @brief Controller helper facade returned by `Twin::controller()`. */
class TwinControllerHandle
{
public:
    explicit TwinControllerHandle(Twin twin);

    /** Create a keyboard teleop controller (bindings from KeyboardBindings::build() or equivalent). */
    KeyboardTeleop keyboard(const std::vector<KeyBinding>& bindings, double step = 0.05, int rate_hz = 20,
                            bool fetch_initial = true, bool verbose = true) const;

private:
    Twin twin_;
};

} // namespace cyberwave

#endif // CYBERWAVE_KEYBOARD_H
