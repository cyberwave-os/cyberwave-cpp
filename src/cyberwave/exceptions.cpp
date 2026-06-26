#include "cyberwave/exceptions.h"

namespace cyberwave
{

CyberwaveError::CyberwaveError(std::string message) : message_(std::move(message)) {}

const char* CyberwaveError::what() const noexcept { return message_.c_str(); }

CyberwaveAPIError::CyberwaveAPIError(std::string message, int status_code)
    : CyberwaveError(std::move(message)), status_code_(status_code)
{
}

CyberwaveInsufficientCreditsError::CyberwaveInsufficientCreditsError(std::string message, double balance,
                                                                     bool manual_block, std::string manual_block_reason)
    : CyberwaveAPIError(std::move(message), 402), balance_(balance), manual_block_(manual_block),
      manual_block_reason_(std::move(manual_block_reason))
{
}

CyberwaveConnectionError::CyberwaveConnectionError(std::string message) : CyberwaveError(std::move(message)) {}

CyberwaveTimeoutError::CyberwaveTimeoutError(std::string message) : CyberwaveConnectionError(std::move(message)) {}

CyberwaveValidationError::CyberwaveValidationError(std::string message) : CyberwaveError(std::move(message)) {}

CyberwaveMQTTError::CyberwaveMQTTError(std::string message) : CyberwaveError(std::move(message)) {}

} // namespace cyberwave
