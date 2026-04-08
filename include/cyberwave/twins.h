/**
 * @brief Twin management helpers for listing, creating, updating, and querying twins.
 */

#ifndef CYBERWAVE_TWINS_H
#define CYBERWAVE_TWINS_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;
class Twin;

/**
 * @brief Manager for twin CRUD, calibration, frame, and schema operations.
 */
class TwinManager
{
public:
    /**
     * @brief Construct a twin manager bound to a client.
     * @param client Owning SDK client.
     */
    explicit TwinManager(const Client& client);

    /**
     * @brief List twins, optionally filtered by environment UUID.
     * @param environment_id Optional environment UUID filter.
     * @return Matching twin handles.
     */
    std::vector<Twin> list(const std::string& environment_id = "") const;

    /**
     * @brief Fetch a twin by UUID.
     * @param twin_id Twin UUID.
     * @return Twin handle populated from backend state.
     */
    Twin get(const std::string& twin_id) const;

    /**
     * @brief Create a twin from an asset in an environment.
     * @return Newly created twin handle.
     */
    Twin create(const std::string& asset_id, const std::string& environment_id, const std::string& name = "",
                const std::string& description = "", const std::vector<double>& position = {},
                const std::vector<double>& orientation = {}, bool fixed_base = false) const;

    /** @brief Update a twin's basic metadata. */
    Twin update(const std::string& twin_id, const std::string& name = "", const std::string& description = "") const;

    /**
     * @brief Update a twin and optionally dock or undock it from another twin.
     * @param twin_id Twin UUID.
     * @param name Optional replacement name.
     * @param description Optional replacement description.
     * @param attach_to_twin_uuid Parent twin UUID, or empty string to undock.
     * @param attach_to_link Optional parent link name.
     * @param offset_position Optional local offset position.
     * @param offset_rotation Optional local offset rotation.
     * @return Updated twin handle.
     */
    Twin update(const std::string& twin_id, const std::string& name, const std::string& description,
                const std::string& attach_to_twin_uuid, const std::string& attach_to_link = "",
                const std::vector<double>& offset_position = {}, const std::vector<double>& offset_rotation = {}) const;

    /**
     * @brief Update a twin's full pose in one call.
     * @return Updated twin handle.
     */
    Twin update_state(const std::string& twin_id, double position_x, double position_y, double position_z,
                      double rotation_w = 1.0, double rotation_x = 0.0, double rotation_y = 0.0,
                      double rotation_z = 0.0) const;

    /** Update only position; leaves rotation unchanged. */
    Twin update_position(const std::string& twin_id, double x, double y, double z) const;

    /** Update only rotation (quaternion); leaves position unchanged. */
    Twin update_rotation(const std::string& twin_id, double w, double rx, double ry, double rz) const;

    /** @brief Delete a twin by UUID. */
    void delete_twin(const std::string& twin_id) const;

    /**
     * Get joint states for a twin.
     * Returns a map from joint name to position (radians).
     * Mirrors Python TwinManager.get_joint_states().
     */
    std::map<std::string, double> get_joint_states(const std::string& twin_id) const;

    /**
     * Update a single joint state.
     * Mirrors Python TwinManager.update_joint_state().
     */
    void update_joint_state(const std::string& twin_id, const std::string& joint_name, double position,
                            double velocity = 0.0, double effort = 0.0) const;

    /**
     * Get joint calibration for a twin. Returns JSON string of the calibration schema.
     * robot_type is passed as query param when non-empty (e.g. "amr", "arm").
     * Note: robot_type query param is not exposed by the generated REST client; it is accepted
     * for compatibility but has no effect on the request.
     * Mirrors Python TwinManager.get_calibration().
     */
    std::string get_calibration(const std::string& twin_id, const std::string& robot_type = "") const;

    /**
     * Update joint calibration for a twin. calibration_json is a JSON object string.
     * robot_type is optional. Returns the updated calibration as JSON.
     * Mirrors Python TwinManager.update_calibration().
     */
    std::string update_calibration(const std::string& twin_id, const std::string& calibration_json,
                                   const std::string& robot_type = "") const;

    /**
     * Get the latest RGB frame for a twin.
     * sensor_id selects a specific camera on multi-camera twins (e.g. "wrist_camera").
     * source_type can request simulation frames when set to "sim".
     * Mirrors Python TwinManager.get_latest_frame().
     */
    std::vector<unsigned char> get_latest_frame(const std::string& twin_id, bool mock = false,
                                                const std::string& sensor_id = "",
                                                const std::string& source_type = "") const;

    /**
     * Get the universal schema (or a sub-path) for a twin.
     * Returns a JSON string. path is the JSON Pointer path (e.g. "/properties/color").
     * Mirrors Python TwinManager.get_universal_schema_at_path().
     */
    std::string get_universal_schema_at_path(const std::string& twin_id, const std::string& path = "") const;

    /**
     * Patch the universal schema for a twin at the given path.
     * Returns the updated schema as a JSON string.
     * Mirrors Python TwinManager.patch_universal_schema().
     */
    std::string patch_universal_schema(const std::string& twin_id, const std::string& path,
                                       const std::string& value_json, const std::string& op = "replace") const;

    /**
     * @brief Return the owning client.
     * @return Client pointer used by this manager.
     */
    const Client& client() const noexcept { return client_.get(); }

private:
    std::reference_wrapper<const Client> client_;
};

} // namespace cyberwave

#endif // CYBERWAVE_TWINS_H
