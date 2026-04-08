#ifndef CYBERWAVE_DATA_H
#define CYBERWAVE_DATA_H

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace cyberwave
{

constexpr const char* DATA_CONTENT_TYPE_NUMPY = "numpy/ndarray";
constexpr const char* DATA_CONTENT_TYPE_JSON = "application/json";
constexpr const char* DATA_CONTENT_TYPE_BYTES = "application/octet-stream";

using DataBytes = std::vector<std::uint8_t>;

struct NdarraySample
{
    DataBytes payload;
    std::vector<std::size_t> shape;
    std::string dtype;
};

using DataValue = std::variant<nlohmann::json, DataBytes, NdarraySample>;

struct DataHeaderMeta
{
    std::string content_type;
    double ts{0.0};
    std::int64_t seq{0};
    std::optional<std::vector<std::size_t>> shape;
    std::optional<std::string> dtype;
    nlohmann::json metadata = nlohmann::json::object();
};

DataBytes encode_data_sample(const DataHeaderMeta& header, const DataBytes& payload);
std::pair<DataHeaderMeta, DataBytes> decode_data_sample(const DataBytes& raw);

class DataHeaderTemplate
{
public:
    DataHeaderTemplate(std::string content_type, std::optional<std::vector<std::size_t>> shape = std::nullopt,
                       std::optional<std::string> dtype = std::nullopt,
                       nlohmann::json metadata = nlohmann::json::object());

    DataBytes pack(const DataBytes& payload, std::optional<double> ts = std::nullopt);

    const std::string& content_type() const noexcept { return content_type_; }
    const std::optional<std::vector<std::size_t>>& shape() const noexcept { return shape_; }
    const std::optional<std::string>& dtype() const noexcept { return dtype_; }
    std::int64_t seq() const;

private:
    std::string content_type_;
    std::optional<std::vector<std::size_t>> shape_;
    std::optional<std::string> dtype_;
    nlohmann::json metadata_;
    DataBytes cached_json_bytes_;
    std::uint32_t cached_header_len_{0};
    mutable std::mutex seq_mutex_;
    std::int64_t seq_{0};
};

struct DataSample
{
    std::string channel;
    DataBytes payload;
    double timestamp{0.0};
    nlohmann::json metadata = nlohmann::json::object();
};

class DataSubscription
{
public:
    virtual ~DataSubscription() = default;
    virtual void close() = 0;
};

class DataBackend
{
public:
    using Callback = std::function<void(const DataSample&)>;

    virtual ~DataBackend() = default;

    virtual void publish(const std::string& channel, const DataBytes& payload,
                         const nlohmann::json& metadata = nlohmann::json::object()) = 0;
    virtual std::shared_ptr<DataSubscription> subscribe(const std::string& channel, Callback callback,
                                                        const std::string& policy = "latest") = 0;
    virtual std::optional<DataSample> latest(const std::string& channel, double timeout_s = 1.0) = 0;
    virtual void close() = 0;

    static void validate_policy(const std::string& policy);
};

class FilesystemDataBackend : public DataBackend
{
public:
    explicit FilesystemDataBackend(std::string base_dir = "", std::size_t ring_buffer_size = 100,
                                   double poll_interval_s = 0.05);
    ~FilesystemDataBackend() override;

    void publish(const std::string& channel, const DataBytes& payload,
                 const nlohmann::json& metadata = nlohmann::json::object()) override;
    std::shared_ptr<DataSubscription> subscribe(const std::string& channel, Callback callback,
                                                const std::string& policy = "latest") override;
    std::optional<DataSample> latest(const std::string& channel, double timeout_s = 1.0) override;
    void close() override;

    const std::string& base_dir() const noexcept { return base_dir_; }

private:
    struct Watcher;

    std::string base_dir_;
    std::size_t ring_buffer_size_;
    double poll_interval_s_;
    std::mutex watchers_mutex_;
    std::vector<std::shared_ptr<Watcher>> watchers_;
    bool closed_{false};
};

std::string build_data_key(const std::string& twin_uuid, const std::string& channel,
                           std::optional<std::string> sensor_name = std::nullopt, const std::string& prefix = "cw");
bool is_valid_data_key(const std::string& key);

class DataBus
{
public:
    using Callback = std::function<void(const DataValue&)>;

    DataBus(std::shared_ptr<DataBackend> backend, std::string twin_uuid,
            std::optional<std::string> sensor_name = std::nullopt, std::string key_prefix = "cw");

    const std::shared_ptr<DataBackend>& backend() const noexcept { return backend_; }
    const std::string& key_prefix() const noexcept { return key_prefix_; }

    void publish(const std::string& channel, const nlohmann::json& sample,
                 const nlohmann::json& metadata = nlohmann::json::object());
    void publish_bytes(const std::string& channel, const DataBytes& sample,
                       const nlohmann::json& metadata = nlohmann::json::object());
    void publish_bytes(const std::string& channel, const std::string& sample,
                       const nlohmann::json& metadata = nlohmann::json::object());
    void publish_ndarray(const std::string& channel, const DataBytes& sample, std::vector<std::size_t> shape,
                         std::string dtype, const nlohmann::json& metadata = nlohmann::json::object());

    std::shared_ptr<DataSubscription> subscribe(const std::string& channel, Callback callback,
                                                const std::string& policy = "latest");
    std::shared_ptr<DataSubscription> subscribe_raw(const std::string& channel, DataBackend::Callback callback,
                                                    const std::string& policy = "latest");

    std::optional<DataValue> latest(const std::string& channel, double timeout_s = 1.0,
                                    std::optional<double> max_age_ms = std::nullopt) const;
    std::optional<DataSample> latest_raw(const std::string& channel, double timeout_s = 1.0) const;

    void close() const;

private:
    std::shared_ptr<DataHeaderTemplate>
    get_or_create_template(const std::string& channel, const std::string& content_type, const nlohmann::json& metadata,
                           std::optional<std::vector<std::size_t>> shape = std::nullopt,
                           std::optional<std::string> dtype = std::nullopt);
    DataValue decode_value(const DataBytes& wire_bytes) const;
    std::string key_for_channel(const std::string& channel) const;

    std::shared_ptr<DataBackend> backend_;
    std::string twin_uuid_;
    std::optional<std::string> sensor_name_;
    std::string key_prefix_;
    mutable std::mutex templates_mutex_;
    std::unordered_map<std::string, std::shared_ptr<DataHeaderTemplate>> templates_;
};

} // namespace cyberwave

#endif // CYBERWAVE_DATA_H
