/**
 * @brief Workspace views and workspace management helpers for the Cyberwave SDK.
 */

#ifndef CYBERWAVE_WORKSPACES_H
#define CYBERWAVE_WORKSPACES_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;

/**
 * @brief Lightweight view over a workspace returned by the backend.
 */
class Workspace
{
public:
    /** @brief Construct a workspace view from a backend payload. */
    explicit Workspace(std::shared_ptr<void> schema_ptr);

    /** @brief Return the workspace UUID. */
    std::string uuid() const;

    /** @brief Return the workspace name. */
    std::string name() const;

    /** @brief Return the workspace description. */
    std::string description() const;

    /** @brief Return the workspace slug. */
    std::string slug() const;

private:
    std::shared_ptr<void> schema_;
};

/**
 * @brief Manager for workspace CRUD operations.
 */
class WorkspaceManager
{
public:
    /**
     * @brief Construct a workspace manager bound to a client.
     * @param client Owning SDK client.
     */
    explicit WorkspaceManager(const Client& client);

    /** @brief List all visible workspaces. */
    std::vector<Workspace> list() const;

    /** @brief Fetch a workspace by UUID. */
    Workspace get(const std::string& workspace_id) const;

    /** @brief Create a new workspace. */
    Workspace create(const std::string& name, const std::string& description = "") const;

    /** @brief Update a workspace's mutable fields. */
    Workspace update(const std::string& workspace_id, const std::string& name = "", const std::string& description = "",
                     const std::string& slug = "") const;

private:
    std::reference_wrapper<const Client> client_;
};

} // namespace cyberwave

#endif // CYBERWAVE_WORKSPACES_H
