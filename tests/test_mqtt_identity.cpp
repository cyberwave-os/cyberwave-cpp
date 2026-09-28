// Keeps the C++ mirror of mqtt_identity honest. The fixtures below are the
// Python SDK's output for the same inputs: a username that does not hash to
// its token is refused at CONNECT, so the two implementations agreeing is the
// whole contract.

#include "cyberwave/config.h"
#include "cyberwave/mqtt_identity.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace cyberwave;

namespace
{

// hashlib.sha256(b"abc").hexdigest()
constexpr const char* SHA256_OF_ABC = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
// hashlib.sha256(b"api_key_secret").hexdigest(), the Python SDK's own fixture
constexpr const char* SHA256_OF_API_KEY_SECRET = "789e45bf946d654ae0e4764dfc1677bbc2aeb7fb609059ae97f216d5420b2f29";

void test_hex_digest_matches_python()
{
    assert(token_sha256("abc") == SHA256_OF_ABC);
    assert(token_sha256("api_key_secret") == SHA256_OF_API_KEY_SECRET);
    std::cout << "  token_sha256 matches Python\n";
}

void test_username_carries_the_hash_and_never_the_token()
{
    const std::string token = "api_key_secret";
    const std::string username = mqtt_username_for_token(token);

    assert(username == std::string("cwh_") + SHA256_OF_API_KEY_SECRET);
    assert(username.find(token) == std::string::npos);
    std::cout << "  username carries the hash, not the token\n";
}

void test_the_derived_username_is_not_the_placeholder()
{
    assert(mqtt_username_for_token("api_key_secret") != std::string(DEFAULT_MQTT_USERNAME));
    std::cout << "  derived username replaces the shared placeholder\n";
}

void test_an_api_token_is_not_a_legacy_broker_password()
{
    // The cloud node scopes a workload token by setting CYBERWAVE_API_KEY and
    // CYBERWAVE_MQTT_PASSWORD to it; read as a broker password it left the
    // worker on the placeholder, which the ACL callback cannot resolve.
    const std::string token = "cw_" + std::string(64, 'a');
    assert(is_api_token(token));
    assert(!is_legacy_broker_password(token));
    std::cout << "  an API token under the broker password still derives\n";
}

void test_a_static_broker_password_stays_legacy()
{
    assert(is_legacy_broker_password("mqttcyb231"));
    assert(is_legacy_broker_password("test"));
    // Only a leading prefix marks a token.
    assert(is_legacy_broker_password("xcw_abc"));
    assert(!is_legacy_broker_password(""));
    std::cout << "  a static broker password keeps the account name\n";
}

} // namespace

int main()
{
    std::cout << "test_mqtt_identity\n";
    test_hex_digest_matches_python();
    test_username_carries_the_hash_and_never_the_token();
    test_the_derived_username_is_not_the_placeholder();
    test_an_api_token_is_not_a_legacy_broker_password();
    test_a_static_broker_password_stays_legacy();
    std::cout << "test_mqtt_identity: all passed\n";
    return 0;
}
