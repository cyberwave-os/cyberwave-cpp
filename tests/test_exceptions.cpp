/**
 * Symmetric with Python: exception hierarchy, what(), status_code on APIError.
 */
#include <cassert>
#include <cyberwave/exceptions.h>
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

int main()
{
    test_base();
    test_api_error();
    test_connection_error();
    test_timeout_error();
    test_validation_error();
    test_mqtt_error();
    return 0;
}
