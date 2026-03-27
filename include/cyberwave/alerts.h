/**
 * @brief Alert views and twin-scoped alert management for the Cyberwave SDK.
 */

#ifndef CYBERWAVE_ALERTS_H
#define CYBERWAVE_ALERTS_H

#include "cyberwave/twin.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;

/**
 * @brief View over a single alert and its lifecycle actions.
 *
 * Instances are typically obtained from `TwinAlertManager::list()`,
 * `TwinAlertManager::create()`, or `TwinAlertManager::get()`.
 */
class Alert
{
public:
    /** @brief Construct an alert view from a backend payload. */
    Alert(const Client& client, std::shared_ptr<void> alert_schema_ptr);

    Alert(Alert&&) noexcept = default;
    Alert& operator=(Alert&&) noexcept = default;
    Alert(const Alert&) = default;
    Alert& operator=(const Alert&) = default;

    /** @brief Return the alert UUID. */
    std::string uuid() const;

    /** @brief Return the human-readable alert name. */
    std::string name() const;

    /** @brief Return the alert description. */
    std::string description() const;

    /** @brief Return the supporting media URL, or an empty string when unset. */
    std::string media() const;

    /** @brief Return the machine-readable alert type. */
    std::string alert_type() const;

    /** @brief Return the severity string. */
    std::string severity() const;

    /** @brief Return the current alert status. */
    std::string status() const;

    /** @brief Return the recorded source type. */
    std::string source_type() const;

    /** @brief Return the associated twin UUID. */
    std::string twin_uuid() const;

    /** @brief Return the associated environment UUID, if any. */
    std::string environment_uuid() const;

    /** @brief Return the associated workflow UUID, if any. */
    std::string workflow_uuid() const;

    /** @brief Return the associated workspace UUID, if any. */
    std::string workspace_uuid() const;

    /** @brief Return the creation timestamp. */
    std::string created_at() const;

    /** @brief Return the last update timestamp. */
    std::string updated_at() const;

    /** @brief Return the resolution timestamp, if any. */
    std::string resolved_at() const;

    /** @brief Return alert metadata as string pairs. */
    std::map<std::string, std::string> metadata() const;

    /** @brief Acknowledge the alert and refresh local state. */
    Alert& acknowledge();

    /** @brief Resolve the alert and refresh local state. */
    Alert& resolve();

    /** @brief Silence the alert and refresh local state. */
    Alert& silence();

    /**
     * @brief Press a custom alert action button by index.
     * @param button_index Zero-based button index.
     * @throws CyberwaveError If `button_index` is negative.
     */
    Alert& press_button(int button_index);

    /** @brief Refresh this alert from the backend. */
    Alert& refresh();

    /** @brief Delete the alert from the backend. */
    void delete_alert();

    struct UpdateOptions
    {
        std::optional<std::string> name;
        std::optional<std::string> description;
        std::optional<std::string> media;
        std::optional<std::string> alert_type;
        std::optional<std::string> severity;
        std::optional<std::string> status;
        std::optional<std::map<std::string, std::string>> metadata;
    };

    /**
     * @brief Update mutable alert fields.
     * @param opts Field set describing which values to send.
     * @return Reference to the updated alert.
     */
    Alert& update(const UpdateOptions& opts);

private:
    std::reference_wrapper<const Client> client_;
    std::shared_ptr<void> schema_; // holds shared_ptr<AlertSchema> in .cpp
};

/**
 * @brief Twin-scoped manager for creating and querying alerts.
 */
class TwinAlertManager
{
public:
    /**
     * @brief Construct a manager scoped to a single twin.
     * @param twin Twin that owns the alerts namespace.
     */
    explicit TwinAlertManager(std::shared_ptr<const Twin> twin);

    struct CreateOptions
    {
        std::string description;
        std::string media;
        std::string alert_type;
        std::string severity{"warning"};
        std::string source_type{"edge"};
        std::string environment_uuid;
        std::string workflow_uuid;
        std::string workspace_uuid;
        std::map<std::string, std::string> metadata;
        bool force{false};
    };

    /** @brief Create an alert with default options. */
    Alert create(const std::string& name);

    /** @brief Create an alert with explicit options. */
    Alert create(const std::string& name, const CreateOptions& options);

    /** @brief Fetch an alert by UUID. */
    Alert get(const std::string& uuid) const;

    struct ListOptions
    {
        std::string status;
        std::string severity;
        int limit{100};
    };

    /** @brief List alerts for the scoped twin using default filters. */
    std::vector<Alert> list() const;

    /** @brief List alerts for the scoped twin using explicit filters. */
    std::vector<Alert> list(const ListOptions& options) const;

private:
    std::shared_ptr<const Twin> twin_;
};

} // namespace cyberwave

#endif // CYBERWAVE_ALERTS_H
