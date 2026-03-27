/*
    alerts.cpp — Alerts example

    Mirrors: examples/alerts.py

    Demonstrates:
     - Creating an alert on a twin
     - Listing alerts
     - Fetching an alert by UUID
     - Resolving an alert

    Before running:
        export CYBERWAVE_API_KEY=your_token_here
*/

#include <cyberwave/alerts.h>
#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/twin.h>
#include <cyberwave/twins.h>

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    try
    {
        cyberwave::Config cfg;
        cfg.load_from_environment();
        cyberwave::Client cw(cfg);

        // Discover available twins and use the first one
        auto twins = cw.twins().list();
        if (twins.empty())
        {
            std::cout << "No twins found. Create one in the Cyberwave dashboard first.\n";
            return 0;
        }
        const std::string& twin_uuid = twins[0].uuid();
        std::cout << "Using twin: " << twins[0].name() << " (" << twin_uuid << ")\n";

        cyberwave::Twin robot = twins[0];
        auto alerts_mgr = robot.alerts();

        // Create an alert on the twin
        cyberwave::TwinAlertManager::CreateOptions opts;
        opts.description = "Needs calibration";
        opts.workspace_uuid = cw.config().workspace_id;
        opts.environment_uuid = robot.environment_id();
        auto alert = alerts_mgr.create("Calibration Needed", opts);
        std::cout << "Created alert: " << alert.uuid() << "\n";

        // List all alerts for this twin
        auto all_alerts = alerts_mgr.list();
        std::cout << all_alerts.size() << " alert(s):\n";
        for (const auto& a : all_alerts)
            std::cout << "  " << a.uuid() << " — " << a.name() << "\n";

        // Fetch the alert by UUID
        auto fetched = alerts_mgr.get(alert.uuid());
        std::cout << "Fetched: " << fetched.name() << "\n";

        // Wait briefly then resolve
        std::this_thread::sleep_for(std::chrono::seconds(2));
        fetched.resolve();
        std::cout << "Alert resolved\n";
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
