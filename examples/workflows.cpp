/*
    workflows.cpp — Workflow automation example

    Mirrors: examples/workflows.py

    Demonstrates:
     - Listing available workflows
     - Triggering an active workflow with typed inputs
     - Waiting for a run to complete with timeout
     - Browsing past runs filtered by status

    Before running:
        export CYBERWAVE_API_KEY=your_token_here
*/

#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/workflows.h>

#include <iostream>
#include <vector>

int main()
{
    try
    {
        cyberwave::Config cfg;
        cfg.load_from_environment();
        cyberwave::Client cw(cfg);

        // --- List available workflows ---
        auto workflows = cw.workflows().list();
        for (const auto& wf : workflows)
            std::cout << wf.name() << " (" << wf.uuid() << ") — " << wf.status() << "\n";

        if (workflows.empty())
        {
            std::cout << "No workflows found. Create one in the Cyberwave dashboard first.\n";
            return 0;
        }

        // --- Trigger the first active workflow ---
        std::vector<cyberwave::Workflow> active;
        for (const auto& wf : workflows)
            if (wf.is_active())
                active.push_back(wf);

        if (active.empty())
        {
            std::cout << "No active workflows. Activate one in the dashboard.\n";
            return 0;
        }

        const auto& workflow = active[0];
        std::cout << "\nTriggering '" << workflow.name() << "' ...\n";

        // Trigger with JSON inputs (mirrors Python workflow.trigger(inputs={...}))
        auto run = workflow.trigger_with_json(R"({"target_position": [1.0, 2.0, 0.0], "speed": 0.5})");
        std::cout << "Run started: " << run.uuid() << "  (status: " << run.status() << ")\n";

        // --- Wait for the run to finish ---
        try
        {
            run.wait(120.0, 3.0);
        }
        catch (const cyberwave::CyberwaveTimeoutError& e)
        {
            std::cout << "Timed out: " << e.what() << "\n";
            return 1;
        }

        std::cout << "\nFinal status : " << run.status() << "\n";
        auto dur = run.duration();
        if (dur >= 0.0)
            std::cout << "Duration     : " << dur << "s\n";

        auto result = run.result_json();
        if (!result.empty())
            std::cout << "Result       : " << result << "\n";
        auto err = run.error();
        if (!err.empty())
            std::cout << "Error        : " << err << "\n";

        // --- Browse past successful runs ---
        auto past_runs = workflow.runs("success");
        std::cout << "\n" << past_runs.size() << " successful past run(s) for '" << workflow.name() << "'\n";

        cw.disconnect();
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
