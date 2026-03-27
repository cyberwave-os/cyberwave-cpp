/**
 * @brief Project views and project management helpers for the Cyberwave SDK.
 */

#ifndef CYBERWAVE_PROJECTS_H
#define CYBERWAVE_PROJECTS_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;

/**
 * @brief Lightweight view over a project returned by the backend.
 */
class Project
{
public:
    /** @brief Construct a project view from a backend payload. */
    explicit Project(std::shared_ptr<void> schema_ptr);

    /** @brief Return the project UUID. */
    std::string uuid() const;

    /** @brief Return the project name. */
    std::string name() const;

    /** @brief Return the project description. */
    std::string description() const;

    /** @brief Return the owning workspace UUID. */
    std::string workspace_uuid() const;

private:
    std::shared_ptr<void> schema_;
};

/**
 * @brief Manager for project CRUD operations.
 */
class ProjectManager
{
public:
    /** @brief Construct a project manager bound to a client. */
    explicit ProjectManager(const Client& client);

    /** @brief List projects, optionally filtered by workspace UUID. */
    std::vector<Project> list(const std::string& workspace_id = "") const;

    /** @brief Fetch a project by UUID. */
    Project get(const std::string& project_id) const;

    /** @brief Create a new project in a workspace. */
    Project create(const std::string& name, const std::string& workspace_id, const std::string& description = "") const;

    /** @brief Update mutable project fields. */
    Project update(const std::string& project_id, const std::string& name = "",
                   const std::string& description = "") const;

    /** @brief Delete a project by UUID. */
    void delete_project(const std::string& project_id) const;

private:
    std::reference_wrapper<const Client> client_;
};

} // namespace cyberwave

#endif // CYBERWAVE_PROJECTS_H
