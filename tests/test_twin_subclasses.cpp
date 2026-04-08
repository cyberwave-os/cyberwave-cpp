/**
 * Tests for twin subclasses and create_twin factory (aligned with Python twin subclasses).
 */

#include "cyberwave/client.h"
#include "cyberwave/config.h"
#include "cyberwave/twin_subclasses.h"

#include <cassert>
#include <memory>

int main()
{
    cyberwave::Config config;
    config.api_key = "test-key";
    cyberwave::Client client(config);

    // Base twin from capabilities all false
    {
        cyberwave::Capabilities caps;
        auto t = cyberwave::create_twin(client, "uuid-base", "base", caps);
        assert(t != nullptr);
        assert(t->uuid() == "uuid-base");
        assert(t->name() == "base");
        assert(dynamic_cast<cyberwave::Twin*>(t.get()) != nullptr);
        assert(dynamic_cast<cyberwave::CameraTwin*>(t.get()) == nullptr);
    }

    // CameraTwin from has_sensors
    {
        cyberwave::Capabilities caps;
        caps.has_sensors = true;
        auto t = cyberwave::create_twin(client, "uuid-cam", "cam", caps);
        assert(t != nullptr);
        assert(t->uuid() == "uuid-cam");
        auto* cam = dynamic_cast<cyberwave::CameraTwin*>(t.get());
        assert(cam != nullptr);
        cam->stop_streaming(); // no-op
        bool threw = false;
        try
        {
            cam->start_streaming(30, 0);
        }
        catch (...)
        {
            threw = true;
        }
        assert(threw);
    }

    // LocomoteTwin from can_locomote
    {
        cyberwave::Capabilities caps;
        caps.can_locomote = true;
        auto t = cyberwave::create_twin(client, "uuid-loco", "loco", caps);
        assert(t != nullptr);
        auto* loco = dynamic_cast<cyberwave::LocomoteTwin*>(t.get());
        assert(loco != nullptr);
        loco->move_forward(1.0);
        loco->turn_left(0.5);
    }

    // FlyingTwin from can_fly
    {
        cyberwave::Capabilities caps;
        caps.can_fly = true;
        auto t = cyberwave::create_twin(client, "uuid-fly", "fly", caps);
        assert(t != nullptr);
        auto* fly = dynamic_cast<cyberwave::FlyingTwin*>(t.get());
        assert(fly != nullptr);
        fly->takeoff(1.0);
        fly->hover();
        fly->land();
    }

    // GripperTwin from can_grip
    {
        cyberwave::Capabilities caps;
        caps.can_grip = true;
        auto t = cyberwave::create_twin(client, "uuid-grip", "grip", caps);
        assert(t != nullptr);
        auto* grip = dynamic_cast<cyberwave::GripperTwin*>(t.get());
        assert(grip != nullptr);
        grip->grip(0.8);
        grip->release();
    }

    // Priority: can_fly wins over can_locomote
    {
        cyberwave::Capabilities caps;
        caps.can_fly = true;
        caps.can_locomote = true;
        auto t = cyberwave::create_twin(client, "u", "n", caps);
        assert(dynamic_cast<cyberwave::FlyingTwin*>(t.get()) != nullptr);
    }

    // New symmetric tests ---

    // has_depth creates DepthCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.has_depth = true;
        auto t = cyberwave::create_twin(client, "uuid-depth", "depth", caps);
        assert(t != nullptr);
        auto* depth = dynamic_cast<cyberwave::DepthCameraTwin*>(t.get());
        assert(depth != nullptr);
        // DepthCameraTwin is-a CameraTwin
        auto* cam = dynamic_cast<cyberwave::CameraTwin*>(t.get());
        assert(cam != nullptr);
    }

    // has_depth + has_sensors → DepthCameraTwin (depth wins over sensors)
    {
        cyberwave::Capabilities caps;
        caps.has_depth = true;
        caps.has_sensors = true;
        auto t = cyberwave::create_twin(client, "uuid-d", "d", caps);
        assert(dynamic_cast<cyberwave::DepthCameraTwin*>(t.get()) != nullptr);
        // Must NOT be a plain CameraTwin (would be wrong type)
        // dynamic_cast to CameraTwin still succeeds because DepthCameraTwin IS-A CameraTwin,
        // but the concrete type is DepthCameraTwin:
        assert(dynamic_cast<cyberwave::DepthCameraTwin*>(t.get()) != nullptr);
    }

    // can_grip + has_sensors → GripperCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.can_grip = true;
        caps.has_sensors = true;
        auto t = cyberwave::create_twin(client, "uuid-gs", "gs", caps);
        auto* grip = dynamic_cast<cyberwave::GripperCameraTwin*>(t.get());
        assert(grip != nullptr);
        assert(dynamic_cast<cyberwave::DepthCameraTwin*>(t.get()) == nullptr);
    }

    // can_fly + has_depth → FlyingDepthCameraTwin (combo)
    {
        cyberwave::Capabilities caps;
        caps.can_fly = true;
        caps.has_depth = true;
        auto t = cyberwave::create_twin(client, "uuid-fd", "fd", caps);
        assert(dynamic_cast<cyberwave::FlyingDepthCameraTwin*>(t.get()) != nullptr);
        assert(dynamic_cast<cyberwave::DepthCameraTwin*>(t.get()) != nullptr);
        auto* fly = dynamic_cast<cyberwave::FlyingDepthCameraTwin*>(t.get());
        fly->takeoff(1.5);
        fly->hover();
        fly->land();
    }

    // can_fly + has_sensors → FlyingCameraTwin (combo)
    {
        cyberwave::Capabilities caps;
        caps.can_fly = true;
        caps.has_sensors = true;
        auto t = cyberwave::create_twin(client, "uuid-fc", "fc", caps);
        assert(dynamic_cast<cyberwave::FlyingCameraTwin*>(t.get()) != nullptr);
        assert(dynamic_cast<cyberwave::CameraTwin*>(t.get()) != nullptr);
    }

    // has_sensors only → CameraTwin (not DepthCameraTwin)
    {
        cyberwave::Capabilities caps;
        caps.has_sensors = true;
        auto t = cyberwave::create_twin(client, "uuid-s", "s", caps);
        assert(dynamic_cast<cyberwave::CameraTwin*>(t.get()) != nullptr);
        assert(dynamic_cast<cyberwave::DepthCameraTwin*>(t.get()) == nullptr);
    }

    // --- Combo class tests ---

    // GripperCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.can_grip = true;
        caps.has_sensors = true;
        auto t = cyberwave::create_twin(client, "uuid-gc", "gc", caps);
        auto* gc = dynamic_cast<cyberwave::GripperCameraTwin*>(t.get());
        assert(gc != nullptr);
        assert(dynamic_cast<cyberwave::CameraTwin*>(gc) != nullptr);
        gc->grip(0.5);
        gc->release();
    }

    // GripperDepthCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.can_grip = true;
        caps.has_depth = true;
        auto t = cyberwave::create_twin(client, "uuid-gdc", "gdc", caps);
        auto* gdc = dynamic_cast<cyberwave::GripperDepthCameraTwin*>(t.get());
        assert(gdc != nullptr);
        assert(dynamic_cast<cyberwave::DepthCameraTwin*>(gdc) != nullptr);
        gdc->grip(1.0);
        gdc->release();
    }

    // LocomoteCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.can_locomote = true;
        caps.has_sensors = true;
        auto t = cyberwave::create_twin(client, "uuid-lc", "lc", caps);
        auto* lc = dynamic_cast<cyberwave::LocomoteCameraTwin*>(t.get());
        assert(lc != nullptr);
        assert(dynamic_cast<cyberwave::CameraTwin*>(lc) != nullptr);
        lc->move_forward(1.0);
        lc->turn_right(0.5);
    }

    // LocomoteDepthCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.can_locomote = true;
        caps.has_depth = true;
        auto t = cyberwave::create_twin(client, "uuid-ldc", "ldc", caps);
        auto* ldc = dynamic_cast<cyberwave::LocomoteDepthCameraTwin*>(t.get());
        assert(ldc != nullptr);
        assert(dynamic_cast<cyberwave::DepthCameraTwin*>(ldc) != nullptr);
        ldc->move_backward(0.5);
        ldc->turn_left(1.0);
    }

    // LocomoteGripperTwin
    {
        cyberwave::Capabilities caps;
        caps.can_locomote = true;
        caps.can_grip = true;
        auto t = cyberwave::create_twin(client, "uuid-lg", "lg", caps);
        auto* lg = dynamic_cast<cyberwave::LocomoteGripperTwin*>(t.get());
        assert(lg != nullptr);
        assert(dynamic_cast<cyberwave::LocomoteTwin*>(lg) != nullptr);
        lg->move_forward(1.0);
        lg->grip(0.8);
        lg->release();
    }

    // LocomoteGripperCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.can_locomote = true;
        caps.can_grip = true;
        caps.has_sensors = true;
        auto t = cyberwave::create_twin(client, "uuid-lgc", "lgc", caps);
        auto* lgc = dynamic_cast<cyberwave::LocomoteGripperCameraTwin*>(t.get());
        assert(lgc != nullptr);
        assert(dynamic_cast<cyberwave::CameraTwin*>(lgc) != nullptr);
        lgc->move_forward(0.5);
        lgc->grip(1.0);
    }

    // LocomoteGripperDepthCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.can_locomote = true;
        caps.can_grip = true;
        caps.has_depth = true;
        auto t = cyberwave::create_twin(client, "uuid-lgdc", "lgdc", caps);
        auto* lgdc = dynamic_cast<cyberwave::LocomoteGripperDepthCameraTwin*>(t.get());
        assert(lgdc != nullptr);
        assert(dynamic_cast<cyberwave::DepthCameraTwin*>(lgdc) != nullptr);
        lgdc->turn_left(0.3);
        lgdc->release();
    }

    // FlyingGripperTwin
    {
        cyberwave::Capabilities caps;
        caps.can_fly = true;
        caps.can_grip = true;
        auto t = cyberwave::create_twin(client, "uuid-fg", "fg", caps);
        auto* fg = dynamic_cast<cyberwave::FlyingGripperTwin*>(t.get());
        assert(fg != nullptr);
        assert(dynamic_cast<cyberwave::FlyingTwin*>(fg) != nullptr);
        fg->takeoff(2.0);
        fg->grip(0.5);
        fg->release();
        fg->land();
    }

    // FlyingGripperCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.can_fly = true;
        caps.can_grip = true;
        caps.has_sensors = true;
        auto t = cyberwave::create_twin(client, "uuid-fgc", "fgc", caps);
        auto* fgc = dynamic_cast<cyberwave::FlyingGripperCameraTwin*>(t.get());
        assert(fgc != nullptr);
        assert(dynamic_cast<cyberwave::CameraTwin*>(fgc) != nullptr);
        fgc->hover();
        fgc->grip(0.7);
    }

    // FlyingGripperDepthCameraTwin
    {
        cyberwave::Capabilities caps;
        caps.can_fly = true;
        caps.can_grip = true;
        caps.has_depth = true;
        auto t = cyberwave::create_twin(client, "uuid-fgdc", "fgdc", caps);
        auto* fgdc = dynamic_cast<cyberwave::FlyingGripperDepthCameraTwin*>(t.get());
        assert(fgdc != nullptr);
        assert(dynamic_cast<cyberwave::DepthCameraTwin*>(fgdc) != nullptr);
        fgdc->takeoff(1.0);
        fgdc->release();
    }

    return 0;
}
