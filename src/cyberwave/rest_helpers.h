#pragma once

#include "CppRestOpenAPIClient/AnyType.h"
#include "CppRestOpenAPIClient/ApiConfiguration.h"
#include "cyberwave/client.h"
#include "cyberwave/client_internal.h"
#include "cyberwave/exceptions.h"

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cyberwave::detail
{

struct RawHttpResponse
{
    web::http::status_code status_code{};
    std::vector<unsigned char> body;
    web::http::http_headers headers;

    std::string text() const
    {
        return utility::conversions::to_utf8string(utility::string_t(body.begin(), body.end()));
    }
};

inline utility::string_t to_utility(const std::string& value) { return utility::conversions::to_string_t(value); }

inline std::string from_utility(const utility::string_t& value)
{
#if defined(_TURN_OFF_PLATFORM_STRING)
    return value;
#else
    return utility::conversions::to_utf8string(value);
#endif
}

inline org::openapitools::client::api::ApiConfiguration* require_api_config(const Client& client)
{
    auto* config = ClientAccess::api_config(&client);
    if (!config)
    {
        throw CyberwaveError("Client has no REST API (missing api_key)");
    }
    return config;
}

inline RawHttpResponse request_raw(const Client& client, const utility::string_t& path, const utility::string_t& method,
                                   const std::map<utility::string_t, utility::string_t>& query = {},
                                   const std::optional<web::json::value>& json_body = std::nullopt,
                                   const std::map<utility::string_t, utility::string_t>& extra_headers = {})
{
    auto* config = require_api_config(client);
    web::http::client::http_client http_client(config->getBaseUrl(), config->getHttpConfig());

    web::http::uri_builder uri(path);
    for (const auto& [key, value] : query)
    {
        uri.append_query(key, value);
    }

    web::http::http_request request(method);
    request.set_request_uri(uri.to_uri());

    for (const auto& [key, value] : config->getDefaultHeaders())
    {
        request.headers().add(key, value);
    }
    for (const auto& [key, value] : extra_headers)
    {
        request.headers().remove(key);
        request.headers().add(key, value);
    }

    if (json_body.has_value())
    {
        request.headers().set_content_type(to_utility("application/json"));
        request.set_body(*json_body);
    }

    auto response = http_client.request(request).get();
    RawHttpResponse result{
        response.status_code(),
        response.extract_vector().get(),
        response.headers(),
    };

    if (result.status_code >= web::http::status_codes::BadRequest)
    {
        const std::string message =
            result.body.empty() ? ("HTTP " + std::to_string(result.status_code)) : result.text();
        throw CyberwaveAPIError(message, static_cast<int>(result.status_code));
    }

    return result;
}

inline web::json::value parse_json_response(const RawHttpResponse& response)
{
    if (response.body.empty())
    {
        return web::json::value::null();
    }
    return web::json::value::parse(to_utility(response.text()));
}

inline std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>>
json_object_to_any_map(const web::json::value& value)
{
    std::map<utility::string_t, std::shared_ptr<org::openapitools::client::model::AnyType>> result;
    if (!value.is_object())
    {
        return result;
    }
    for (const auto& [key, item] : value.as_object())
    {
        auto any_value = std::make_shared<org::openapitools::client::model::AnyType>();
        any_value->fromJson(item);
        result[key] = any_value;
    }
    return result;
}

} // namespace cyberwave::detail
