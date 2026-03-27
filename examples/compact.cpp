/*
    compact.cpp — Minimal SDK usage

    Mirrors: examples/compact.py

    Demonstrates:
     - Loading credentials from environment variables
     - Listing twins and reading all joint positions for the first one

    Before running:
        export CYBERWAVE_API_KEY=your_token_here
        # Optional: export CYBERWAVE_BASE_URL=http://localhost:8000
*/

#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/joints.h>
#include <cyberwave/twin.h>
#include <cyberwave/twins.h>

#include <iostream>

int main()
{
    try
    {
        // Credentials loaded from CYBERWAVE_API_KEY / CYBERWAVE_BASE_URL env vars
        cyberwave::Config cfg;
        cfg.load_from_environment();
        cyberwave::Client cw(cfg);

        // List available twins and pick the first one
        auto twins = cw.twins().list();
        if (twins.empty())
        {
            std::cout << "No twins found. Create one in the Cyberwave dashboard first.\n";
            return 0;
        }

        const auto& first = twins[0];
        std::cout << "Using twin: " << first.name() << " (" << first.uuid() << ")\n";

        cyberwave::Twin robot = cw.twin(first.uuid());

        // Print all current joint positions
        auto joint_positions = robot.joints().get_all();
        if (joint_positions.empty())
            std::cout << "(no joint states available)\n";
        for (const auto& [name, pos] : joint_positions)
            std::cout << name << ": " << pos << " rad\n";
    }
    catch (const cyberwave::CyberwaveError& e)
    {
        std::cerr << "SDK error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
