/**
 * Keyboard teleop: KeyboardBindings, TwinControllerHandle, keyboard() (no run/apply_key with backend).
 */
#include <cassert>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/keyboard.h>
#include <cyberwave/twin.h>
#include <string>
#include <vector>

using namespace cyberwave;

static void test_keyboard_bindings_build()
{
    KeyboardBindings b;
    b.bind("W", "arm_joint", "increase").bind("S", "arm_joint", "decrease");
    std::vector<KeyBinding> out = b.build();
    assert(out.size() == 2);
    assert(out[0].key == "W" && out[0].joint_name == "arm_joint" && out[0].direction == "increase");
    assert(out[1].key == "S" && out[1].joint_name == "arm_joint" && out[1].direction == "decrease");
}

static void test_controller_keyboard_construction()
{
    Config cfg;
    cfg.api_key = "";
    Client c(cfg);
    Twin t = c.twin("twin-123");
    TwinControllerHandle h = t.controller();
    std::vector<KeyBinding> bindings;
    bindings.push_back({"W", "j1", "increase"});
    KeyboardTeleop teleop = h.keyboard(bindings, 0.05, 20, false, false);
    (void)teleop;
}

static void test_bindings_validation()
{
    KeyboardBindings b;
    try
    {
        b.bind("K", "j", "invalid");
        assert(false);
    }
    catch (const CyberwaveValidationError&)
    {
    }
}

int main()
{
    test_keyboard_bindings_build();
    test_controller_keyboard_construction();
    test_bindings_validation();
    return 0;
}
