#include "cyberwave/exceptions.h"

namespace cyberwave
{

CyberwaveError::CyberwaveError(std::string message) : message_(std::move(message)) {}

const char* CyberwaveError::what() const noexcept { return message_.c_str(); }

CyberwaveAPIError::CyberwaveAPIError(std::string message, int status_code)
    : CyberwaveError(std::move(message)), status_code_(status_code)
{
}

CyberwaveConnectionError::CyberwaveConnectionError(std::string message) : CyberwaveError(std::move(message)) {}

CyberwaveTimeoutError::CyberwaveTimeoutError(std::string message) : CyberwaveConnectionError(std::move(message)) {}

CyberwaveValidationError::CyberwaveValidationError(std::string message) : CyberwaveError(std::move(message)) {}

CyberwaveMQTTError::CyberwaveMQTTError(std::string message) : CyberwaveError(std::move(message)) {}

} // namespace cyberwave
