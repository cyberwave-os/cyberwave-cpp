/**
 * @brief Derivation of the MQTT username from the API token that authenticates it.
 *
 * The broker sends the password only on CONNECT; every later authorization
 * check carries the username and client id alone. A username derived from the
 * token lets the backend identify the session from the check itself, instead of
 * remembering it in a slot shared by every client that connects as `mqttcyb`.
 *
 * The username is echoed into broker logs and authorization callbacks, so it is
 * the hash, never the token: it identifies, the password still authenticates.
 *
 * Mirrored in `cyberwave-backend/src/lib/mqtt_identity.py` and
 * `cyberwave-sdks/cyberwave-python/cyberwave/mqtt_identity.py`. The three must
 * agree exactly -- a username that does not hash to its token is refused at
 * CONNECT.
 *
 * `is_api_token` separates the two kinds of password a client may hold: a
 * Cyberwave API token, which the username is derived from, and a static broker
 * account password, which leaves the account name standing.
 */

#ifndef CYBERWAVE_MQTT_IDENTITY_H
#define CYBERWAVE_MQTT_IDENTITY_H

#include <array>
#include <cstdio>
#include <openssl/sha.h>
#include <string>

namespace cyberwave
{

/** @brief Namespace marker on a token-derived username; not a secret. */
constexpr const char* MQTT_TOKEN_USERNAME_PREFIX = "cwh_";

/** @brief Prefix every Cyberwave API token carries (backend `APIToken.generate_token`). */
constexpr const char* API_TOKEN_PREFIX = "cw_";

/** @brief Whether @p credential is an API token rather than a broker password. */
inline bool is_api_token(const std::string& credential) { return credential.rfind(API_TOKEN_PREFIX, 0) == 0; }

/** @brief Whether @p password is a static broker account password (set, and not an API token). */
inline bool is_legacy_broker_password(const std::string& password)
{
    return !password.empty() && !is_api_token(password);
}

/** @brief Lowercase hex sha256 of @p token, matching Python's `hexdigest()`. */
inline std::string token_sha256(const std::string& token)
{
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
    SHA256(reinterpret_cast<const unsigned char*>(token.data()), token.size(), digest.data());

    std::string hex;
    hex.reserve(digest.size() * 2);
    for (const unsigned char byte : digest)
    {
        char pair[3];
        std::snprintf(pair, sizeof(pair), "%02x", byte);
        hex.append(pair, 2);
    }
    return hex;
}

/** @brief The MQTT username a client holding @p token should connect under. */
inline std::string mqtt_username_for_token(const std::string& token)
{
    return std::string(MQTT_TOKEN_USERNAME_PREFIX) + token_sha256(token);
}

} // namespace cyberwave

#endif // CYBERWAVE_MQTT_IDENTITY_H
