/*
    mixed_example.cpp

    Retrieve the list of twins in a given environment using the REST API,
    then actuate all their joints via the MQTT batch helper
    `update_joint_states(...)` implemented in the MQTT client.

    Key actions demonstrated:
     - Use the generated REST SDK (DefaultApi) to list and fetch twins
     - Build per-twin joint maps and subscribe to twin topics for visibility
     - Use the MQTT batch helper to publish joint updates for each twin

    Usage:
        ./mixed_example <environment_uuid>
        or set EXAMPLE_ENV_UUID environment variable
*/

#include "mqtt_client.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <thread>

// REST SDK includes
#include "CppRestOpenAPIClient/ApiClient.h"
#include "CppRestOpenAPIClient/ApiConfiguration.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"

using namespace org::openapitools::client::api;
using namespace org::openapitools::client::model;
namespace conversions = utility::conversions;

using namespace cyberwave;

static volatile std::sig_atomic_t g_running = 1;
void handle_sigint(int) { g_running = 0; }

static std::string getenv_or(const char* name, const std::string& def = "")
{
    const char* v = std::getenv(name);
    return v ? std::string(v) : def;
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

// Simple oscillation helper used to vary joint positions over time
static void oscillate_joint_position(JointState& joint)
{
    const double PI = 3.14159265358979323846;
    static const double delta = 0.05;
    static int dir = 1;

    if (!joint.position.has_value())
    {
        joint.position = -PI;
        dir = 1;
    }
    else
    {
        double v = joint.position.value() + dir * delta;
        if (v >= PI)
        {
            v = PI;
            dir = -1;
        }
        else if (v <= -PI)
        {
            v = -PI;
            dir = 1;
        }
        joint.position = v;
    }
}

int main(int argc, char** argv)
{
    // Environment UUID identifies which environment's twins we will actuate
    std::string env_uuid = (argc >= 2) ? argv[1] : getenv_or("EXAMPLE_ENV_UUID", getenv_or("ENVIRONMENT", ""));
    if (env_uuid.empty())
    {
        std::cerr << "Environment UUID required (argv[1] or EXAMPLE_ENV_UUID)" << std::endl;
        return 2;
    }

    // REST config
    const auto base_url = getenv_or("CYBERWAVE_BASE_URL", "http://localhost:8000");
    const auto api_key = getenv_or("CYBERWAVE_API_KEY", "test-api-key");
    const auto mqtt_api_token = getenv_or("CYBERWAVE_API_KEY");
    if (mqtt_api_token.empty())
    {
        std::cerr << "CYBERWAVE_API_KEY is required for MQTT authentication" << std::endl;
        return 2;
    }

    // MQTT config
    CyberwaveConfig mqtt_cfg{.mqtt_host = getenv_or("CYBERWAVE_MQTT_HOST", "mqtt.cyberwave.com"),
                             .mqtt_port = 1883,
                             .mqtt_username = getenv_or("CYBERWAVE_MQTT_USERNAME", "mqttcyb"),
                             .mqtt_api_token = mqtt_api_token};

    std::signal(SIGINT, handle_sigint);

    try
    {
        // Create REST API client and list all twins, then filter by environment uuid
        auto api = make_default_api(base_url, api_key);
        auto twins_task = api->srcAppApiTwinsListAllTwins();
        auto twins = twins_task.get();

        std::vector<std::shared_ptr<TwinSchema>> matching_twins;
        for (const auto& t : twins)
        {
            if (!t)
                continue;
            if (t->environmentUuidIsSet() && conversions::to_utf8string(t->getEnvironmentUuid()) == env_uuid)
                matching_twins.push_back(t);
        }

        std::cout << "Found " << matching_twins.size() << " twin(s) in environment " << env_uuid << std::endl;
        if (matching_twins.empty())
            return 0;

        // Build MQTT client
        CyberwaveMQTTClient client(mqtt_cfg);
        client.connect();

        // subscribe callback to print inbound messages
        auto print_cb = [](const std::string& topic, const json& message)
        {
            // std::cout << "[RECV] " << topic << " -> " << message.dump() << std::endl;
        };

        // Pre-create a per-twin joint map (name -> JointState) by fetching twin details
        std::map<std::string, std::map<std::string, JointState>> twin_joints;

        for (const auto& twin : matching_twins)
        {
            // fetch details for joint keys
            auto twin_details_task = api->srcAppApiTwinsGetTwin(twin->getUuid());
            auto twin_details = twin_details_task.get();
            if (!twin_details)
                continue;

            auto joint_states = twin_details->getJointStates();
            std::map<std::string, JointState> joints_for_twin;
            for (const auto& kv : joint_states)
            {
                try
                {
                    std::string joint_name = conversions::to_utf8string(kv.first);
                    JointState s;
                    s.position = 0.0;
                    s.velocity = 0.0;
                    s.effort = 0.0;
                    joints_for_twin[joint_name] = s;
                }
                catch (...)
                {
                }
            }

            // If there are no named joints, create a default single joint "1"
            if (joints_for_twin.empty())
                joints_for_twin["1"] = JointState{};

            // Subscribe to topics for visibility
            const std::string twin_uuid = conversions::to_utf8string(twin->getUuid());
            client.subscribe_twin_position(twin_uuid, print_cb);
            client.subscribe_twin_rotation(twin_uuid, print_cb);
            client.subscribe_joint_states(twin_uuid, print_cb);

            twin_joints[conversions::to_utf8string(twin->getUuid())] = std::move(joints_for_twin);
        }

        // small delay to allow subscriptions to register
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::cout << "Starting actuation loop (Ctrl+C to stop)..." << std::endl;
        while (g_running && client.is_connected())
        {
            // For each twin, oscillate joint positions and send batch updates
            for (auto& [twin_uuid, joints] : twin_joints)
            {
                for (auto& kv : joints)
                    oscillate_joint_position(kv.second);

                // Use the MQTT batch helper to publish all joints for this twin
                client.update_joint_states(twin_uuid, joints);
                // std::cout << "Published " << joints.size() << " joint updates for twin " << twin_uuid << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        std::cout << "Shutting down..." << std::endl;
        client.disconnect();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
