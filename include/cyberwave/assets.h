/**
 * @brief Asset views and asset management helpers for the Cyberwave SDK.
 */

#ifndef CYBERWAVE_ASSETS_H
#define CYBERWAVE_ASSETS_H

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;

/**
 * @brief Typed controller policy reference returned by controller setup views.
 */
struct PolicyRefPayload
{
    std::string kind;
    std::string value;

    bool empty() const noexcept { return kind.empty() || value.empty(); }
};

/**
 * @brief Backend-resolved runtime target for a controller policy.
 */
struct ControlRuntimeTargetPayload
{
    bool enabled{false};
    std::string runtime_kind;
    std::string backend;
    std::string adapter;
    std::string source_type;
    std::string safety_level;
    std::string input_contract;
    std::string output_contract;

    bool empty() const noexcept { return runtime_kind.empty() || backend.empty(); }
};

/**
 * @brief Typed view over the backend-resolved asset controller setup response.
 */
class AssetControllerSetupView
{
public:
    /** @brief Build a setup view from the raw backend JSON payload. */
    static AssetControllerSetupView from_json(std::string json);

    /** @brief Return the original JSON payload. */
    const std::string& raw_json() const noexcept { return raw_json_; }

    /** @brief Return the setup asset UUID. */
    std::string asset_uuid() const;

    /** @brief Return the primary preview controller key, if present. */
    std::string primary_controller_key() const;

    /** @brief Return the recommended primary policy reference, if present. */
    PolicyRefPayload primary_policy_ref() const;

    /** @brief Return a default policy reference for a runtime/backend pair. */
    PolicyRefPayload default_policy_ref(const std::string& runtime_kind, const std::string& backend) const;

    /** @brief Return the resolved runtime target for a runtime/backend pair. */
    ControlRuntimeTargetPayload runtime_target(const std::string& runtime_kind, const std::string& backend) const;

    /** @brief Return the number of backend-resolved runtime policy rows. */
    std::size_t runtime_policy_count() const;

private:
    explicit AssetControllerSetupView(std::string json);

    std::string raw_json_;
};

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

    /** @brief Return the canonical registry identifier. */
    std::string registry_id() const;

    /** @brief Return the first-class registry alias, if present. */
    std::string registry_id_alias() const;

    /** @brief Return whether the asset defaults to a fixed base. */
    bool fixed_base() const;

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
     * @brief List assets.
     * @param workspace_id Legacy parameter kept for source compatibility. The
     * backend no longer supports workspace filtering on this endpoint.
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

    /**
     * Rebuild the authoritative universal schema from source files.
     * Mirrors Python AssetManager.rebuild_universal_schema().
     */
    std::string rebuild_universal_schema(const std::string& asset_id, bool sync = false) const;

    /**
     * Return the backend-resolved controller setup view for an asset.
     *
     * This uses the same typed resolver as the frontend and Python SDK, so
     * callers do not need to inspect raw asset/controller metadata. The JSON
     * includes configured controller bindings, primary controller selection,
     * runtime policies, runtime options, typed policy_ref entries, and
     * recommended setup defaults.
     */
    std::string get_controller_setup(const std::string& asset_id) const;

    /**
     * Return a typed wrapper around the backend-resolved controller setup view.
     */
    AssetControllerSetupView get_controller_setup_view(const std::string& asset_id) const;

private:
    std::reference_wrapper<const Client> client_;
};

} // namespace cyberwave

#endif // CYBERWAVE_ASSETS_H
