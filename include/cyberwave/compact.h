/**
 * @brief Compact module-style helpers around a global SDK client.
 *
 * Useful for small scripts that prefer `configure()` and `twin()` over
 * explicitly storing a `Client` instance.
 */

#ifndef CYBERWAVE_COMPACT_H
#define CYBERWAVE_COMPACT_H

#include "cyberwave/config.h"
#include "cyberwave/twin.h"

#include <string>

namespace cyberwave
{

class Client; // forward declaration for get_client()

/**
 * @brief Configure the global client from a copied configuration.
 * @param config SDK configuration to apply globally.
 */
void configure(const Config& config);

/**
 * @brief Configure the global client from a moved configuration.
 * @param config SDK configuration to apply globally.
 */
void configure(Config&& config);

/**
 * @brief Return the configured global client.
 * @return Reference to the global client instance.
 * @throws CyberwaveError If `configure()` has not been called yet.
 */
Client& get_client();

/**
 * @brief Return a twin handle from the global client.
 * @param slug Twin slug or UUID.
 * @return Twin handle from `get_client().twin(slug)`.
 * @throws CyberwaveError If the global client has not been configured.
 */
Twin twin(const std::string& slug);

} // namespace cyberwave

#endif // CYBERWAVE_COMPACT_H
