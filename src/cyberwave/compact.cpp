#include "cyberwave/compact.h"
#include "cyberwave/client.h"
#include "cyberwave/exceptions.h"

#include <memory>
#include <mutex>

namespace cyberwave
{

namespace
{

std::mutex g_mutex;
std::unique_ptr<Client> g_client;

} // namespace

void configure(const Config& config)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_client)
    {
        g_client->disconnect();
        g_client.reset();
    }
    g_client = std::make_unique<Client>(config);
}

void configure(Config&& config)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_client)
    {
        g_client->disconnect();
        g_client.reset();
    }
    g_client = std::make_unique<Client>(std::move(config));
}

Client& get_client()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_client)
        throw CyberwaveError("cyberwave client not configured; call cyberwave::configure(config) first");
    return *g_client;
}

Twin twin(const std::string& identifier, const TwinResolveOptions& options)
{
    return get_client().twin(identifier, options);
}

} // namespace cyberwave
