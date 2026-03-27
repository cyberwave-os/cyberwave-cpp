/*
    rest_example.cpp

    Quickstart example that demonstrates usage of the generated C++ REST SDK
    (CppRestOpenAPIClient). The program shows how to configure the client
    using environment variables, call API endpoints (list workspaces, list and
    fetch digital twins), and how to safely extract heterogeneous values
    (generated client uses `AnyType` to represent flexible JSON values).

    Key actions demonstrated:
     - Read `CYBERWAVE_BASE_URL` and `CYBERWAVE_API_KEY` from the environment
     - Construct `ApiConfiguration` and `DefaultApi`
     - List workspaces and list/fetch digital twins
     - Inspect `TwinSchema::getJointStates()` and convert `AnyType` values

    Before running: set `CYBERWAVE_BASE_URL` and `CYBERWAVE_API_KEY` in your
    environment to point to a running Cyberwave backend. Example:
        export CYBERWAVE_BASE_URL=http://localhost:8000
        export CYBERWAVE_API_KEY=your_token_here
*/

#include <cstdlib>
#include <iostream>
#include <string>

#include "CppRestOpenAPIClient/ApiClient.h"
#include "CppRestOpenAPIClient/ApiConfiguration.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"

using namespace org::openapitools::client::api;
using namespace org::openapitools::client::model;

namespace conversions = utility::conversions;

// Load configuration from environment variables with sensible defaults
static std::pair<std::string, std::string> load_config_from_env()
{
    const auto env_base = std::getenv("CYBERWAVE_BASE_URL");
    const auto env_token = std::getenv("CYBERWAVE_API_KEY");

    auto base_url = env_base ? std::string(env_base) : std::string("http://localhost:8000");
    auto token = env_token ? std::string(env_token) : std::string("test-api-key");
    return {base_url, token};
}

// Create an ApiConfiguration and DefaultApi pointer from base url and token
static std::shared_ptr<DefaultApi> make_default_api(const std::string& base_url, const std::string& token)
{
    auto config = std::make_shared<ApiConfiguration>();
    config->setBaseUrl(conversions::to_string_t(base_url));
    if (!token.empty())
        config->getDefaultHeaders()[U("Authorization")] = conversions::to_string_t("Token " + token);

    auto apiClient = std::make_shared<ApiClient>(config);
    return std::make_shared<DefaultApi>(apiClient);
}

static double anyTypeToDouble(const std::shared_ptr<AnyType>& a, double fallback = NAN)
{
    if (!a)
        return fallback;

    web::json::value v = a->toJson();

    if (v.is_number())
        return v.as_double();
    if (v.is_string())
    {
        try
        {
            return std::stod(utility::conversions::to_utf8string(v.as_string()));
        }
        catch (...)
        {
            return fallback;
        }
    }
    if (v.is_object())
    {
        if (v.has_field(U("value")) && v.at(U("value")).is_number())
            return v.at(U("value")).as_double();
        if (v.has_field(U("position")) && v.at(U("position")).is_number())
            return v.at(U("position")).as_double();
    }
    return fallback;
}

// Orchestrates the quickstart flow; returns exit code (0 success, non-zero
// failure)
int run_quickstart()
{
    auto [base_url, token] = load_config_from_env();

    std::cout << "Starting REST quickstart\n";
    std::cout << "  Base URL: " << base_url << "\n";
    std::cout << "  Token:    " << (token.size() ? (token.substr(0, 8) + "...") : "(none)") << "\n";

    try
    {
        auto api = make_default_api(base_url, token);

        // 1. List workspaces
        std::cout << "Listing workspaces...\n";

        auto task_1 = api->srcUsersApiWorkspacesListWorkspaces();
        auto workspaces = task_1.get();

        std::cout << "Retrieved " << workspaces.size() << " workspace(s)\n";
        if (!workspaces.empty() && workspaces[0] && workspaces[0]->nameIsSet())
            std::cout << "First workspace name: " << conversions::to_utf8string(workspaces[0]->getName()) << "\n";

        // 2. Get a digital twin
        auto task_2 = api->srcAppApiTwinsListAllTwins();
        auto list_of_twins = task_2.get();
        std::cout << "Retrieved " << list_of_twins.size() << " digital twin(s)\n";
        size_t idx = 1;
        for (auto twin : list_of_twins)
        {
            if (twin && twin->nameIsSet())
            {
                auto task_3 = api->srcAppApiTwinsGetTwin(twin->getUuid());
                auto twin_details = task_3.get();

                if (twin_details)
                {
                    std::cout << " - Twin " << idx++ << ": " << conversions::to_utf8string(twin->getName())
                              << " with UUID " << conversions::to_utf8string(twin->getUuid()) << "\n";
                    auto joint_states = twin_details->getJointStates();
                    if (!joint_states.empty())
                    {
                        std::cout << "   Joint states:\n";
                        for (const auto& joint : joint_states)
                        {
                            // Print joint name/key if available
                            try
                            {
                                std::cout << "   - " << conversions::to_utf8string(joint.first) << ": ";
                            }
                            catch (...)
                            {
                            }

                            double val = anyTypeToDouble(joint.second, NAN);
                            if (val == val) // check not NaN
                                std::cout << val << " rad \n";
                            else
                            {
                                // Fallback: print raw JSON or string
                                // representation
                                if (joint.second)
                                {
                                    web::json::value v = joint.second->toJson();
                                    if (v.is_string())
                                        std::cout << conversions::to_utf8string(v.as_string()) << "\n";
                                    else
                                        std::cout << conversions::to_utf8string(v.serialize()) << "\n";
                                }
                                else
                                    std::cout << "(null)\n";
                            }
                        }
                    }
                }
            }
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "REST quickstart failed: " << e.what() << "\n";
        return 1;
    }
}

int main() { return run_quickstart(); }
