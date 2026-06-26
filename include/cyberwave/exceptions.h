/**
 * @brief Exception hierarchy used by the Cyberwave C++ SDK.
 */

#ifndef CYBERWAVE_EXCEPTIONS_H
#define CYBERWAVE_EXCEPTIONS_H

#include <exception>
#include <limits>
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
 * @brief Error raised when an account has insufficient credits (HTTP 402).
 *
 * Raised for both balance-exhausted accounts and accounts that have been
 * manually blocked by staff.  Check @c manual_block() to distinguish the two.
 */
class CyberwaveInsufficientCreditsError : public CyberwaveAPIError
{
public:
    /**
     * @brief Construct a credit-exhausted error.
     * @param message Human-readable error message.
     * @param balance  Current credit balance (NaN when unavailable).
     * @param manual_block True when the block was set by staff, not exhaustion.
     * @param manual_block_reason Human-readable reason for a staff-set block.
     */
    CyberwaveInsufficientCreditsError(std::string message, double balance = std::numeric_limits<double>::quiet_NaN(),
                                      bool manual_block = false, std::string manual_block_reason = "");

    /** @brief Current credit balance, or NaN when not available. */
    double balance() const noexcept { return balance_; }

    /** @brief True when the block was imposed by staff rather than by balance. */
    bool manual_block() const noexcept { return manual_block_; }

    /** @brief Human-readable reason for a staff-set block (may be empty). */
    const std::string& manual_block_reason() const noexcept { return manual_block_reason_; }

private:
    double balance_;
    bool manual_block_;
    std::string manual_block_reason_;
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
