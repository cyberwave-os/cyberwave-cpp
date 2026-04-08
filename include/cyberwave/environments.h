/**
 * @brief Environment views and environment management helpers.
 */

#ifndef CYBERWAVE_ENVIRONMENTS_H
#define CYBERWAVE_ENVIRONMENTS_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;
class Twin;

/**
 * @brief Lightweight view over an attachment returned by the backend.
 */
class Attachment
{
public:
    /** @brief Construct an attachment view from a backend payload. */
    explicit Attachment(std::shared_ptr<void> schema_ptr);

    /** @brief Return the attachment UUID. */
    std::string uuid() const;

    /** @brief Return the download URL for the attachment. */
    std::string file_url() const;

    /** @brief Return attachment metadata as a JSON object string. */
    std::string metadata_json() const;

private:
    std::shared_ptr<void> schema_;
};

/**
 * @brief Lightweight view over an environment returned by the backend.
 */
class Environment
{
public:
    /** @brief Construct an environment view from a backend payload. */
    explicit Environment(std::shared_ptr<void> schema_ptr);

    /** @brief Return the environment UUID. */
    std::string uuid() const;

    /** @brief Return the environment name. */
    std::string name() const;

    /** @brief Return the environment description. */
    std::string description() const;

    /** @brief Return the parent project UUID. */
    std::string project_uuid() const;

    /** @brief Return the parent workspace UUID. */
    std::string workspace_uuid() const;

private:
    std::shared_ptr<void> schema_;
};

/**
 * @brief Manager for environment CRUD and export operations.
 */
class EnvironmentManager
{
public:
    /** @brief Construct an environment manager bound to a client. */
    explicit EnvironmentManager(const Client& client);

    /** @brief List environments, optionally filtered by project UUID. */
    std::vector<Environment> list(const std::string& project_id = "") const;

    /** @brief Fetch an environment by UUID. */
    Environment get(const std::string& environment_id) const;

    /** @brief Return the first environment or `std::nullopt` when none exist. */
    std::optional<Environment> get_first_or_none() const;

    /** @brief Create a new environment. */
    Environment create(const std::string& name, const std::string& project_id = "",
                       const std::string& description = "") const;

    /** @brief Delete an environment by UUID. */
    void delete_environment(const std::string& environment_id, const std::string& project_id) const;

    /** @brief Return the twins currently placed in an environment. */
    std::vector<Twin> get_twins(const std::string& environment_id) const;

    /**
     * Get the universal schema for an environment as a raw JSON string.
     * Mirrors Python EnvironmentManager.get_universal_schema_json().
     */
    std::string get_universal_schema_json(const std::string& environment_id) const;

    /**
     * Export environment as URDF scene ZIP bytes.
     * Mirrors Python EnvironmentManager.export_urdf_scene().
     */
    std::vector<unsigned char> export_urdf_scene(const std::string& environment_id) const;

    /**
     * Export environment as MuJoCo scene ZIP bytes.
     * Mirrors Python EnvironmentManager.export_mujoco_scene().
     */
    std::vector<unsigned char> export_mujoco_scene(const std::string& environment_id) const;

    /**
     * Generate and store a static preview attachment for an environment.
     * Mirrors Python EnvironmentManager.create_preview().
     */
    Attachment create_preview(const std::string& environment_id) const;

    /**
     * Replace the environment base universal schema and return the updated schema payload.
     * Mirrors Python EnvironmentManager.set_universal_schema().
     */
    std::string set_universal_schema(const std::string& environment_id, const std::string& schema_json) const;

    /**
     * Patch the environment base universal schema and return the updated schema payload.
     * Mirrors Python EnvironmentManager.patch_universal_schema().
     */
    std::string patch_universal_schema(const std::string& environment_id, const std::string& path,
                                       const std::string& value_json, const std::string& op = "replace") const;

private:
    std::reference_wrapper<const Client> client_;
};

} // namespace cyberwave

#endif // CYBERWAVE_ENVIRONMENTS_H
