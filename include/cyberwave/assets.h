/**
 * @brief Asset views and asset management helpers for the Cyberwave SDK.
 */

#ifndef CYBERWAVE_ASSETS_H
#define CYBERWAVE_ASSETS_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;

/**
 * @brief Lightweight view over an asset returned by the backend.
 */
class Asset
{
public:
    /** @brief Build an asset view from a full asset schema payload. */
    static Asset from_schema(std::shared_ptr<void> schema_ptr);

    /** @brief Build an asset view from a list-item asset schema payload. */
    static Asset from_list_schema(std::shared_ptr<void> schema_ptr);

    /** @brief Return the asset UUID. */
    std::string uuid() const;

    /** @brief Return the asset name. */
    std::string name() const;

    /** @brief Return the asset description. */
    std::string description() const;

private:
    explicit Asset(std::shared_ptr<void> schema_ptr, bool is_list_schema);

    std::shared_ptr<void> schema_;
    bool is_list_schema_;
};

/**
 * @brief Manager for asset CRUD and schema operations.
 */
class AssetManager
{
public:
    /**
     * @brief Construct an asset manager bound to a client.
     * @param client Owning SDK client.
     */
    explicit AssetManager(const Client& client);

    /**
     * @brief List assets, optionally restricted to a workspace.
     * @param workspace_id Optional workspace UUID filter.
     * @return Asset list.
     */
    std::vector<Asset> list(const std::string& workspace_id = "") const;

    /** @brief Fetch a single asset by UUID. */
    Asset get(const std::string& asset_id) const;

    /** @brief Create a new asset record. */
    Asset create(const std::string& name, const std::string& description = "") const;

    /** @brief Update an existing asset. */
    Asset update(const std::string& asset_id, const std::string& name = "", const std::string& description = "") const;

    /** @brief Delete an asset by UUID. */
    void delete_asset(const std::string& asset_id) const;

    /**
     * @brief Upload a GLB file for an existing asset.
     * @param asset_id Asset UUID.
     * @param file_path Local path to the GLB file.
     * @return Updated asset view.
     */
    Asset upload_glb(const std::string& asset_id, const std::string& file_path) const;

    /**
     * Get asset by registry ID. The REST API accepts either UUID or registry ID;
     * this is a convenience alias for get(). Mirrors Python AssetManager.get_by_registry_id().
     */
    Asset get_by_registry_id(const std::string& registry_id) const;

    /**
     * Get the full universal schema for an asset. Returns a JSON string.
     * Mirrors Python AssetManager.get_universal_schema().
     */
    std::string get_universal_schema(const std::string& asset_id) const;

    /**
     * Get the universal schema at a specific path. Returns a JSON string.
     * Mirrors Python AssetManager.get_universal_schema_at_path().
     */
    std::string get_universal_schema_at_path(const std::string& asset_id, const std::string& path = "") const;

    /**
     * Patch the universal schema for an asset. Returns updated schema as JSON.
     * Mirrors Python AssetManager.patch_universal_schema().
     */
    std::string patch_universal_schema(const std::string& asset_id, const std::string& path,
                                       const std::string& value_json, const std::string& op = "replace") const;

private:
    std::reference_wrapper<const Client> client_;
};

} // namespace cyberwave

#endif // CYBERWAVE_ASSETS_H
