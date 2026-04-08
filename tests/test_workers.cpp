#include "cyberwave/client.h"
#include "cyberwave/config.h"
#include "cyberwave/workers.h"

#include <cassert>

using namespace cyberwave;

namespace
{

const std::string kTwinUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";

void test_typed_hook_registration()
{
    HookRegistry registry;

    auto frame = registry.on_frame(kTwinUuid, [](const DataValue&, const HookContext&) {}, "front", 15);
    auto imu = registry.on_imu(kTwinUuid, [](const DataValue&, const HookContext&) {});
    auto lidar = registry.on_lidar(kTwinUuid, [](const DataValue&, const HookContext&) {}, "top");
    auto generic = registry.on_data(kTwinUuid, "custom/channel", [](const DataValue&, const HookContext&) {});

    assert(frame.channel == "frames/front");
    assert(frame.hook_type == "frame");
    assert(frame.sensor_name == "front");
    assert(frame.options.at("fps") == 15);

    assert(imu.channel == "imu");
    assert(imu.hook_type == "imu");

    assert(lidar.channel == "lidar/top");
    assert(lidar.sensor_name == "top");

    assert(generic.channel == "custom/channel");
    assert(generic.hook_type == "data");

    const auto hooks = registry.hooks();
    assert(hooks.size() == 4);
}

void test_synchronized_groups_and_clear()
{
    HookRegistry registry;

    auto group = registry.on_synchronized(
        kTwinUuid, {"frames/front", "depth/front"}, [](const SynchronizedSamples&, const HookContext&) {}, 100.0);

    assert(group.channels.size() == 2);
    assert(group.channels[0] == "frames/front");
    assert(group.tolerance_ms == 100.0);

    const auto groups = registry.synchronized_groups();
    assert(groups.size() == 1);

    registry.clear();
    assert(registry.hooks().empty());
    assert(registry.synchronized_groups().empty());
}

void test_client_hook_registry()
{
    Config cfg;
    cfg.api_key = "";
    Client client(cfg);

    auto& hooks = client.hooks();
    hooks.on_joint_states(kTwinUuid, [](const DataValue&, const HookContext&) {});
    hooks.on_temperature(kTwinUuid, [](const DataValue&, const HookContext&) {});

    const auto registrations = client.hooks().hooks();
    assert(registrations.size() == 2);
    assert(registrations[0].channel == "joint_states");
    assert(registrations[1].channel == "temperature");
}

} // namespace

int main()
{
    test_typed_hook_registration();
    test_synchronized_groups_and_clear();
    test_client_hook_registry();
    return 0;
}
