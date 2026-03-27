/**
 * @brief Exception hierarchy used by the Cyberwave C++ SDK.
 */

#ifndef CYBERWAVE_EXCEPTIONS_H
#define CYBERWAVE_EXCEPTIONS_H

#include <exception>
#include <string>

namespace cyberwave
{

/**
 * @brief Base exception for all Cyberwave SDK errors.
 */
class CyberwaveError : public std::exception
{
public:
    /**
     * @brief Construct a new SDK error.
     * @param message Human-readable error message.
     */
    explicit CyberwaveError(std::string message);

    /** @brief Return the error message. */
    const char* what() const noexcept override;

protected:
    std::string message_;
};

/**
 * @brief Error raised when an API request fails.
 */
class CyberwaveAPIError : public CyberwaveError
{
public:
    /**
     * @brief Construct a new API error.
     * @param message Human-readable error message.
     * @param status_code Optional HTTP status code.
     */
    CyberwaveAPIError(std::string message, int status_code = 0);

    /** @brief Return the HTTP status code, or `0` when unavailable. */
    int status_code() const noexcept { return status_code_; }

private:
    int status_code_;
};

/**
 * @brief Error raised when network-level connectivity fails.
 */
class CyberwaveConnectionError : public CyberwaveError
{
public:
    explicit CyberwaveConnectionError(std::string message);
};

/**
 * @brief Error raised when a request times out.
 */
class CyberwaveTimeoutError : public CyberwaveConnectionError
{
public:
    explicit CyberwaveTimeoutError(std::string message);
};

/**
 * @brief Error raised when client-side validation fails.
 */
class CyberwaveValidationError : public CyberwaveError
{
public:
    explicit CyberwaveValidationError(std::string message);
};

/**
 * @brief Error raised for MQTT-related failures.
 */
class CyberwaveMQTTError : public CyberwaveError
{
public:
    explicit CyberwaveMQTTError(std::string message);
};

} // namespace cyberwave

#endif // CYBERWAVE_EXCEPTIONS_H
