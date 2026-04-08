/**
 * @brief Scene composition helpers for environment-level twin operations.
 */

#ifndef CYBERWAVE_SCENE_H
#define CYBERWAVE_SCENE_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;
class Twin;

/**
 * @brief Environment scene facade for listing, adding, docking, and undocking twins.
 */
class Scene
{
public:
    /**
     * @brief Construct a scene bound to a specific environment.
     * @param client Owning SDK client.
     * @param environment_id Environment UUID.
     */
    Scene(const Client& client, std::string environment_id);

    /** @brief Return the bound environment UUID. */
    const std::string& environment_id() const noexcept { return environment_id_; }

    /** List twins in this environment. */
    std::vector<Twin> get_twins() const;

    /** Add a twin to the scene (creates twin in this environment). */
    Twin add_twin(const std::string& asset_id, const std::string& name = "", const std::string& description = "",
                  const std::vector<double>& position = {},    // [x, y, z]
                  const std::vector<double>& orientation = {}, // [w, x, y, z]
                  bool fixed_base = false) const;

    /** Dock a twin to a parent (attach_to_twin_uuid); optional link name and offsets. */
    Twin dock(const std::string& twin_id, const std::string& parent_twin_id, const std::string& link_name = "",
              const std::vector<double>& offset_position = {},        // [x, y, z]
              const std::vector<double>& offset_rotation = {}) const; // [w, x, y, z]

    /** Undock a twin from its parent. */
    Twin undock(const std::string& twin_id) const;

    /**
     * Get the composed universal schema for this scene's environment.
     * Returns a raw JSON string.
     * Mirrors Python Scene.get_composed_schema().
     */
    std::string get_composed_schema() const;

    /** No-op: next get_twins() always fetches fresh from API. */
    void refresh() const {}

private:
    std::reference_wrapper<const Client> client_;
    std::string environment_id_;
};

} // namespace cyberwave

#endif // CYBERWAVE_SCENE_H
