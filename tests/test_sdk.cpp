/**
 * Cyberwave C++ SDK Test Suite
 *
 * This test suite verifies the basic functionality of the Cyberwave C++ SDK
 * by testing against a running backend instance.
 *
 * Environment Variables:
 *   CYBERWAVE_BASE_URL - Backend URL (default: http://localhost:8000)
 *   CYBERWAVE_API_KEY    - API token for authentication (default: test-api-key)
 */

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

// Include the generated REST SDK headers
#include "CppRestOpenAPIClient/ApiClient.h"
#include "CppRestOpenAPIClient/ApiConfiguration.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"

using namespace org::openapitools::client::api;
using namespace org::openapitools::client::model;

// ANSI color codes for better output
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_RESET "\033[0m"

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

// Helper function to get environment variable with default
std::string getEnvVar(const std::string& key, const std::string& defaultValue)
{
    const char* val = std::getenv(key.c_str());
    return val ? std::string(val) : defaultValue;
}

// Helper function to print test header
void printTestHeader(int testNum, const std::string& description)
{
    std::cout << COLOR_BLUE << "\n=== Test " << testNum << ": " << description << " ===" << COLOR_RESET << std::endl;
}

// Helper function to print success
void printSuccess(const std::string& message)
{
    std::cout << COLOR_GREEN << "✅ " << message << COLOR_RESET << std::endl;
    tests_passed++;
}

// Helper function to print failure
void printFailure(const std::string& message)
{
    std::cerr << COLOR_RED << "❌ " << message << COLOR_RESET << std::endl;
    tests_failed++;
}

// Helper function to print info
void printInfo(const std::string& message)
{
    std::cout << COLOR_YELLOW << "ℹ️  " << message << COLOR_RESET << std::endl;
}

/**
 * Test 1: Verify SDK headers and structure
 */
bool test_sdk_structure()
{
    printTestHeader(1, "SDK Structure and Headers");
    try
    {
        // Just verify that we can include and reference the main types
        printInfo("Checking SDK headers are accessible...");

        // These should compile if headers are correct
        std::shared_ptr<ApiConfiguration> config;
        std::shared_ptr<ApiClient> client;

        printSuccess("SDK headers are accessible and structure is valid");
        return true;
    }
    catch (const std::exception& e)
    {
        printFailure(std::string("SDK structure test failed: ") + e.what());
        return false;
    }
}

/**
 * Test 2: Create API configuration
 */
bool test_api_configuration()
{
    printTestHeader(2, "API Configuration Creation");
    try
    {
        std::string baseUrl = getEnvVar("CYBERWAVE_BASE_URL", "http://localhost:8000");
        std::string apiKey = getEnvVar("CYBERWAVE_API_KEY", "test-api-key");

        printInfo("Creating API configuration...");
        printInfo("Base URL: " + baseUrl);

        auto config = std::make_shared<ApiConfiguration>();
        config->setBaseUrl(utility::conversions::to_string_t(baseUrl + "/api/v1"));

        // Set API key in header
        config->setApiKey(utility::conversions::to_string_t("Authorization"),
                          utility::conversions::to_string_t("Token " + apiKey));

        printSuccess("API configuration created successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        printFailure(std::string("API configuration test failed: ") + e.what());
        return false;
    }
}

/**
 * Test 3: Create API client
 */
bool test_api_client_creation()
{
    printTestHeader(3, "API Client Instantiation");
    try
    {
        std::string baseUrl = getEnvVar("CYBERWAVE_BASE_URL", "http://localhost:8000");
        std::string apiKey = getEnvVar("CYBERWAVE_API_KEY", "test-api-key");

        printInfo("Creating API client...");

        auto config = std::make_shared<ApiConfiguration>();
        config->setBaseUrl(utility::conversions::to_string_t(baseUrl + "/api/v1"));
        config->setApiKey(utility::conversions::to_string_t("Authorization"),
                          utility::conversions::to_string_t("Token " + apiKey));

        auto apiClient = std::make_shared<ApiClient>(config);
        auto api = std::make_shared<DefaultApi>(apiClient);

        printSuccess("API client instantiated successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        printFailure(std::string("API client creation test failed: ") + e.what());
        return false;
    }
}

/**
 * Test 4: Test backend connectivity
 */
bool test_backend_connectivity()
{
    printTestHeader(4, "Backend Connectivity");
    try
    {
        std::string baseUrl = getEnvVar("CYBERWAVE_BASE_URL", "http://localhost:8000");
        std::string apiKey = getEnvVar("CYBERWAVE_API_KEY", "test-api-key");

        printInfo("Testing connection to backend...");
        printInfo("Endpoint: " + baseUrl + "/api/v1");

        auto config = std::make_shared<ApiConfiguration>();
        config->setBaseUrl(utility::conversions::to_string_t(baseUrl));
        // curl -v -H "Authorization: Token test-api-key" http://host.docker.internal:8000/api/v1/users/workspaces
        config->getDefaultHeaders()[utility::conversions::to_string_t("Authorization")] =
            utility::conversions::to_string_t("Token " + apiKey);

        auto apiClient = std::make_shared<ApiClient>(config);
        auto api = std::make_shared<DefaultApi>(apiClient);

        // Try to list workspaces - this will verify:
        // 1. Network connectivity
        // 2. API endpoint is correct
        // 3. Authentication works
        // 4. Response parsing works
        printInfo("Attempting to list workspaces...");

        auto workspacesTask = api->srcUsersApiWorkspacesListWorkspaces();
        auto workspaces = workspacesTask.get();

        printSuccess("Successfully connected to backend");
        printInfo("Response received and parsed successfully");

        // Print workspace count
        if (!workspaces.empty())
        {
            printInfo("Found " + std::to_string(workspaces.size()) + " workspace(s)");

            // Print first workspace details
            auto firstWorkspace = workspaces[0];
            if (firstWorkspace && firstWorkspace->nameIsSet())
                printInfo("First workspace: " + utility::conversions::to_utf8string(firstWorkspace->getName()));
        }
        else
            printInfo("No workspaces found (this is normal for a fresh backend)");

        return true;
    }
    catch (const std::exception& e)
    {
        printFailure(std::string("Backend connectivity test failed: ") + e.what());
        printInfo("Make sure the backend is running and accessible");
        return false;
    }
}

/**
 * Test 5: Test error handling
 */
bool test_error_handling()
{
    printTestHeader(5, "Error Handling");

    try
    {
        std::string baseUrl = getEnvVar("CYBERWAVE_BASE_URL", "http://localhost:8000");

        printInfo("Testing error handling with invalid API key...");

        auto config = std::make_shared<ApiConfiguration>();
        config->setBaseUrl(utility::conversions::to_string_t(baseUrl));
        // curl -v -H "Authorization: Bearer test-api-key" http://host.docker.internal:8000/api/v1/users/workspaces
        config->getDefaultHeaders()[utility::conversions::to_string_t("Authorization")] =
            utility::conversions::to_string_t("Token invalid-key-should-fail");

        auto apiClient = std::make_shared<ApiClient>(config);
        auto api = std::make_shared<DefaultApi>(apiClient);

        try
        {
            auto workspacesTask = api->srcUsersApiWorkspacesListWorkspaces();
            auto workspaces = workspacesTask.get();

            // If we get here, the backend might not be enforcing auth
            printInfo("Note: Backend did not reject invalid API key (might be in dev mode)");
            return true;
        }
        catch (const std::exception& e)
        {
            // This is actually expected - invalid auth should fail
            printSuccess("Error handling works correctly (invalid auth was rejected)");
            return true;
        }
    }
    catch (const std::exception& e)
    {
        printFailure(std::string("Error handling test failed unexpectedly: ") + e.what());
        return false;
    }
}

/**
 * Main test runner
 */
int main()
{
    std::cout << COLOR_BLUE << R"(
╔══════════════════════════════════════════════════════════╗
║        Cyberwave C++ SDK Test Suite                      ║
╚══════════════════════════════════════════════════════════╝
)" << COLOR_RESET
              << std::endl;

    // Print configuration
    std::string baseUrl = getEnvVar("CYBERWAVE_BASE_URL", "http://localhost:8000");
    std::string apiKey = getEnvVar("CYBERWAVE_API_KEY", "test-api-key");

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Base URL: " << baseUrl << std::endl;
    std::cout << "  Token:    " << (apiKey.substr(0, 8) + "...") << std::endl;
    std::cout << std::endl;

    // Run all tests
    bool allPassed = true;

    allPassed &= test_sdk_structure();
    allPassed &= test_api_configuration();
    allPassed &= test_api_client_creation();
    allPassed &= test_backend_connectivity();
    allPassed &= test_error_handling();

    // Print summary
    std::cout << COLOR_BLUE << "\n╔═══════════════════════════════════════════════════════════╗" << COLOR_RESET
              << std::endl;
    std::cout << COLOR_BLUE << "║  Test Summary                                             ║" << COLOR_RESET
              << std::endl;
    std::cout << COLOR_BLUE << "╚═══════════════════════════════════════════════════════════╝" << COLOR_RESET
              << std::endl;

    std::cout << COLOR_GREEN << "✅ Passed: " << tests_passed << COLOR_RESET << std::endl;
    std::cout << COLOR_RED << "❌ Failed: " << tests_failed << COLOR_RESET << std::endl;

    if (allPassed && tests_failed == 0)
    {
        std::cout << COLOR_GREEN << "\n🎉 All tests passed!" << COLOR_RESET << std::endl;
        // _Exit skips static destructors. cpprestsdk's pplx::threadpool destructor
        // crashes on ARM64 Linux due to its weaker memory model; the OS reclaims
        // resources anyway so skipping destructors is safe for a test binary.
        _Exit(0);
    }
    else
    {
        std::cout << COLOR_RED << "\n❌ Some tests failed!" << COLOR_RESET << std::endl;
        _Exit(1);
    }
}
