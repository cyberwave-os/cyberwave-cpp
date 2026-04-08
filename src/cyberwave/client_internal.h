#pragma once

#include "CppRestOpenAPIClient/ApiConfiguration.h"
#include "CppRestOpenAPIClient/api/DefaultApi.h"
#include "cyberwave/client.h"

namespace cyberwave
{

/**
 * @brief Internal bridge for accessing `Client` implementation details inside the SDK.
 */
struct ClientAccess
{
    /**
     * @brief Return the generated REST client for an SDK client.
     * @param client SDK client instance.
     * @return Generated `DefaultApi` pointer, or `nullptr` when REST is not configured.
     */
    static org::openapitools::client::api::DefaultApi* default_api(const Client* client);

    /**
     * @brief Return the generated REST API configuration for an SDK client.
     * @param client SDK client instance.
     * @return Generated `ApiConfiguration` pointer, or `nullptr` when REST is not configured.
     */
    static org::openapitools::client::api::ApiConfiguration* api_config(const Client* client);

    /** @brief Return a weak lifetime token for guarding async callbacks tied to a client. */
    static std::weak_ptr<void> lifetime_token(const Client* client);
};

} // namespace cyberwave
