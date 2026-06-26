/**
 * Symmetric with Python: exception hierarchy, what(), status_code on APIError.
 */
#include <cassert>
#include <cmath>
#include <cyberwave/exceptions.h>
#include <limits>
#include <stdexcept>
#include <string>

using namespace cyberwave;

static void test_base()
{
    CyberwaveError e("base message");
    assert(std::string(e.what()) == "base message");
    assert(dynamic_cast<const std::exception*>(&e));
}

static void test_api_error()
{
    CyberwaveAPIError e("api failed", 404);
    assert(std::string(e.what()) == "api failed");
    assert(e.status_code() == 404);
    assert(dynamic_cast<const CyberwaveError*>(&e));
}

static void test_connection_error()
{
    CyberwaveConnectionError e("connection refused");
    assert(std::string(e.what()) == "connection refused");
    assert(dynamic_cast<const CyberwaveError*>(&e));
}

static void test_timeout_error()
{
    CyberwaveTimeoutError e("timed out");
    assert(std::string(e.what()) == "timed out");
    assert(dynamic_cast<const CyberwaveConnectionError*>(&e));
}

static void test_validation_error()
{
    CyberwaveValidationError e("invalid input");
    assert(std::string(e.what()) == "invalid input");
    assert(dynamic_cast<const CyberwaveError*>(&e));
}

static void test_mqtt_error()
{
    CyberwaveMQTTError e("mqtt disconnected");
    assert(std::string(e.what()) == "mqtt disconnected");
    assert(dynamic_cast<const CyberwaveError*>(&e));
}

static void test_insufficient_credits_error()
{
    // With all fields populated
    CyberwaveInsufficientCreditsError e("credits exhausted", -0.011772, false, "");
    assert(std::string(e.what()) == "credits exhausted");
    assert(e.status_code() == 402);
    assert(e.balance() == -0.011772);
    assert(e.manual_block() == false);
    assert(e.manual_block_reason().empty());
    // Must be catchable as CyberwaveAPIError and CyberwaveError
    assert(dynamic_cast<const CyberwaveAPIError*>(&e));
    assert(dynamic_cast<const CyberwaveError*>(&e));

    // Manual-block variant
    CyberwaveInsufficientCreditsError blocked("org blocked", std::numeric_limits<double>::quiet_NaN(), true, "CI test");
    assert(blocked.status_code() == 402);
    assert(blocked.manual_block() == true);
    assert(blocked.manual_block_reason() == "CI test");
    // balance is NaN when not available
    assert(std::isnan(blocked.balance()));
}

int main()
{
    test_base();
    test_api_error();
    test_connection_error();
    test_timeout_error();
    test_validation_error();
    test_mqtt_error();
    test_insufficient_credits_error();
    return 0;
}
