#include "cyberwave/data.h"

#include "cyberwave/exceptions.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <thread>

namespace cyberwave
{

namespace
{

constexpr std::uint32_t kHeaderLenSize = 4;
constexpr std::uint32_t kTsSeqSize = 16;
constexpr std::uint32_t kMaxHeaderBytes = 64 * 1024;

double now_seconds()
{
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void append_u32_le(DataBytes& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xFFu));
}

std::uint32_t read_u32_le(const DataBytes& raw, std::size_t offset)
{
    return static_cast<std::uint32_t>(raw[offset]) | (static_cast<std::uint32_t>(raw[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(raw[offset + 2]) << 16u) | (static_cast<std::uint32_t>(raw[offset + 3]) << 24u);
}

template <typename T>
void append_pod(DataBytes& out, const T& value)
{
    const auto* begin = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), begin, begin + sizeof(T));
}

template <typename T>
T read_pod(const DataBytes& raw, std::size_t offset)
{
    T value{};
    std::memcpy(&value, raw.data() + offset, sizeof(T));
    return value;
}

void ensure_metadata_object(const nlohmann::json& metadata)
{
    if (!metadata.is_null() && !metadata.is_object())
    {
        throw CyberwaveError("Data metadata must be a JSON object");
    }
}

nlohmann::json build_header_json(const std::string& content_type, const std::optional<std::vector<std::size_t>>& shape,
                                 const std::optional<std::string>& dtype, const nlohmann::json& metadata)
{
    ensure_metadata_object(metadata);
    nlohmann::json json = nlohmann::json::object();
    json["content_type"] = content_type;
    if (shape)
        json["shape"] = *shape;
    if (dtype)
        json["dtype"] = *dtype;
    if (metadata.is_object())
    {
        for (auto it = metadata.begin(); it != metadata.end(); ++it)
        {
            json[it.key()] = it.value();
        }
    }
    return json;
}

std::string safe_channel_name(const std::string& channel)
{
    std::string name;
    name.reserve(channel.size() * 2);
    for (char ch : channel)
    {
        if (ch == '\0')
            continue;
        if (ch == '/')
            name += "__";
        else
            name.push_back(ch);
    }

    std::string normalized;
    normalized.reserve(name.size() * 2);
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        if (i + 1 < name.size() && name[i] == '.' && name[i + 1] == '.')
        {
            normalized += "_dotdot_";
            ++i;
            continue;
        }
        normalized.push_back(name[i]);
    }

    if (normalized.empty())
        return "_empty_";

    bool all_dots = true;
    for (char ch : normalized)
    {
        if (ch != '.')
        {
            all_dots = false;
            break;
        }
    }
    return all_dots ? "_empty_" : normalized;
}

bool is_numeric_sample_file(const std::filesystem::path& path)
{
    if (!path.has_filename() || path.filename() == "latest.bin" || path.extension() != ".bin")
        return false;

    const std::string stem = path.stem().string();
    return !stem.empty() &&
           std::all_of(stem.begin(), stem.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

DataBytes read_file_bytes(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw CyberwaveError("Unable to open file: " + path.string());
    return DataBytes(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void write_file_bytes(const std::filesystem::path& path, const DataBytes& payload)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
        throw CyberwaveError("Unable to open file for writing: " + path.string());
    file.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (!file.good())
        throw CyberwaveError("Failed writing file: " + path.string());
}

std::optional<double> sample_timestamp_from_name(const std::filesystem::path& path)
{
    try
    {
        return static_cast<double>(std::stoll(path.stem().string())) / 1e9;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

const std::regex& uuid_regex()
{
    static const std::regex pattern("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    return pattern;
}

const std::regex& channel_regex()
{
    static const std::regex pattern("^[a-z][a-z0-9_]*(?:/[a-z][a-z0-9_]*)*$");
    return pattern;
}

const std::regex& sensor_regex()
{
    static const std::regex pattern("^[a-z][a-z0-9_]*$");
    return pattern;
}

const std::regex& prefix_regex()
{
    static const std::regex pattern("^[a-z][a-z0-9_]*$");
    return pattern;
}

void validate_data_key_parts(const std::string& twin_uuid, const std::string& channel,
                             const std::optional<std::string>& sensor_name, const std::string& prefix)
{
    if (!std::regex_match(prefix, prefix_regex()))
        throw CyberwaveError("Invalid data key prefix: " + prefix);
    if (!std::regex_match(twin_uuid, uuid_regex()))
        throw CyberwaveError("Invalid twin UUID for DataBus: " + twin_uuid);
    if (!std::regex_match(channel, channel_regex()))
        throw CyberwaveError("Invalid DataBus channel: " + channel);
    if (sensor_name && !std::regex_match(*sensor_name, sensor_regex()))
        throw CyberwaveError("Invalid DataBus sensor name: " + *sensor_name);
}

class FilesystemWatcher : public std::enable_shared_from_this<FilesystemWatcher>
{
public:
    FilesystemWatcher(std::string channel, std::filesystem::path channel_dir, DataBackend::Callback callback,
                      double poll_interval_s, std::string policy)
        : channel_(std::move(channel)), channel_dir_(std::move(channel_dir)), callback_(std::move(callback)),
          poll_interval_s_(poll_interval_s), policy_(std::move(policy))
    {
        if (std::filesystem::exists(channel_dir_))
        {
            for (const auto& entry : std::filesystem::directory_iterator(channel_dir_))
            {
                if (entry.is_regular_file() && is_numeric_sample_file(entry.path()))
                    seen_.insert(entry.path().filename().string());
            }
        }
    }

    ~FilesystemWatcher() { stop(); }

    void start()
    {
        thread_ = std::thread([this]() { run(); });
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();
        if (thread_.joinable())
            thread_.join();
    }

private:
    void run()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!stopped_)
        {
            lock.unlock();
            poll_once();
            lock.lock();
            cv_.wait_for(lock, std::chrono::duration<double>(poll_interval_s_), [this]() { return stopped_; });
        }
    }

    void poll_once()
    {
        if (!std::filesystem::exists(channel_dir_))
            return;

        std::vector<std::filesystem::path> new_files;
        for (const auto& entry : std::filesystem::directory_iterator(channel_dir_))
        {
            if (!entry.is_regular_file() || !is_numeric_sample_file(entry.path()))
                continue;
            const std::string filename = entry.path().filename().string();
            if (seen_.find(filename) == seen_.end())
                new_files.push_back(entry.path());
        }

        if (new_files.empty())
            return;

        std::sort(new_files.begin(), new_files.end());
        std::vector<std::filesystem::path> targets =
            (policy_ == "latest") ? std::vector<std::filesystem::path>{new_files.back()} : new_files;

        for (const auto& path : targets)
        {
            try
            {
                DataSample sample;
                sample.channel = channel_;
                sample.payload = read_file_bytes(path);
                sample.timestamp = sample_timestamp_from_name(path).value_or(now_seconds());

                const std::filesystem::path metadata_path = path.parent_path() / (path.stem().string() + ".meta.json");
                if (std::filesystem::exists(metadata_path))
                {
                    std::ifstream metadata_file(metadata_path);
                    if (metadata_file)
                        metadata_file >> sample.metadata;
                }

                callback_(sample);
            }
            catch (...)
            {
            }
        }

        for (const auto& path : new_files)
            seen_.insert(path.filename().string());
    }

    std::string channel_;
    std::filesystem::path channel_dir_;
    DataBackend::Callback callback_;
    double poll_interval_s_;
    std::string policy_;
    std::set<std::string> seen_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_{false};
    std::thread thread_;
};

class FilesystemSubscriptionImpl : public DataSubscription
{
public:
    explicit FilesystemSubscriptionImpl(std::shared_ptr<FilesystemWatcher> watcher) : watcher_(std::move(watcher)) {}

    void close() override
    {
        if (closed_)
            return;
        closed_ = true;
        if (watcher_)
            watcher_->stop();
    }

private:
    std::shared_ptr<FilesystemWatcher> watcher_;
    bool closed_{false};
};

} // namespace

DataBytes encode_data_sample(const DataHeaderMeta& header, const DataBytes& payload)
{
    const nlohmann::json json = build_header_json(header.content_type, header.shape, header.dtype, header.metadata);
    const std::string json_text = json.dump(-1, ' ', false, nlohmann::json::error_handler_t::strict);
    const std::uint32_t total_header_len = kTsSeqSize + static_cast<std::uint32_t>(json_text.size());
    if (total_header_len > kMaxHeaderBytes)
        throw CyberwaveError("Data header exceeds maximum supported size");

    DataBytes out;
    out.reserve(kHeaderLenSize + total_header_len + payload.size());
    append_u32_le(out, total_header_len);
    append_pod(out, header.ts);
    append_pod(out, header.seq);
    out.insert(out.end(), json_text.begin(), json_text.end());
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

std::pair<DataHeaderMeta, DataBytes> decode_data_sample(const DataBytes& raw)
{
    const std::size_t min_size = kHeaderLenSize + kTsSeqSize;
    if (raw.size() < min_size)
        throw CyberwaveError("Data frame too short");

    const std::uint32_t total_header_len = read_u32_le(raw, 0);
    if (total_header_len > kMaxHeaderBytes)
        throw CyberwaveError("Data header length exceeds maximum supported size");

    const std::size_t header_end = kHeaderLenSize + total_header_len;
    if (header_end > raw.size())
        throw CyberwaveError("Truncated data frame");

    const double ts = read_pod<double>(raw, kHeaderLenSize);
    const std::int64_t seq = read_pod<std::int64_t>(raw, kHeaderLenSize + sizeof(double));
    const auto json_begin = raw.begin() + static_cast<std::ptrdiff_t>(kHeaderLenSize + kTsSeqSize);
    const auto json_end = raw.begin() + static_cast<std::ptrdiff_t>(header_end);

    nlohmann::json header_json;
    try
    {
        header_json = nlohmann::json::parse(json_begin, json_end);
    }
    catch (const nlohmann::json::exception& e)
    {
        throw CyberwaveError(std::string("Invalid data header JSON: ") + e.what());
    }

    if (!header_json.is_object() || !header_json.contains("content_type"))
        throw CyberwaveError("Data header missing required content_type");

    DataHeaderMeta header;
    header.content_type = header_json.at("content_type").get<std::string>();
    header.ts = ts;
    header.seq = seq;

    if (header_json.contains("shape"))
    {
        header.shape = header_json.at("shape").get<std::vector<std::size_t>>();
        header_json.erase("shape");
    }
    if (header_json.contains("dtype"))
    {
        header.dtype = header_json.at("dtype").get<std::string>();
        header_json.erase("dtype");
    }
    header_json.erase("content_type");
    header.metadata = header_json.empty() ? nlohmann::json::object() : header_json;

    DataBytes payload(raw.begin() + static_cast<std::ptrdiff_t>(header_end), raw.end());
    return {header, payload};
}

DataHeaderTemplate::DataHeaderTemplate(std::string content_type, std::optional<std::vector<std::size_t>> shape,
                                       std::optional<std::string> dtype, nlohmann::json metadata)
    : content_type_(std::move(content_type)), shape_(std::move(shape)), dtype_(std::move(dtype)),
      metadata_(std::move(metadata))
{
    const nlohmann::json json = build_header_json(content_type_, shape_, dtype_, metadata_);
    const std::string json_text = json.dump(-1, ' ', false, nlohmann::json::error_handler_t::strict);
    cached_json_bytes_ = DataBytes(json_text.begin(), json_text.end());
    cached_header_len_ = kTsSeqSize + static_cast<std::uint32_t>(cached_json_bytes_.size());
    if (cached_header_len_ > kMaxHeaderBytes)
        throw CyberwaveError("Data header exceeds maximum supported size");
}

DataBytes DataHeaderTemplate::pack(const DataBytes& payload, std::optional<double> ts)
{
    if (!ts)
        ts = now_seconds();

    std::int64_t seq = 0;
    {
        std::lock_guard<std::mutex> lock(seq_mutex_);
        seq = seq_;
        ++seq_;
    }

    DataBytes out;
    out.reserve(kHeaderLenSize + cached_header_len_ + payload.size());
    append_u32_le(out, cached_header_len_);
    append_pod(out, *ts);
    append_pod(out, seq);
    out.insert(out.end(), cached_json_bytes_.begin(), cached_json_bytes_.end());
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

std::int64_t DataHeaderTemplate::seq() const
{
    std::lock_guard<std::mutex> lock(seq_mutex_);
    return seq_;
}

void DataBackend::validate_policy(const std::string& policy)
{
    if (policy != "latest" && policy != "fifo")
        throw std::invalid_argument("Invalid subscribe policy '" + policy + "'");
}

struct FilesystemDataBackend::Watcher
{
    std::shared_ptr<FilesystemWatcher> impl;
};

FilesystemDataBackend::FilesystemDataBackend(std::string base_dir, std::size_t ring_buffer_size, double poll_interval_s)
    : base_dir_(base_dir.empty()
                    ? (std::getenv("CYBERWAVE_DATA_DIR") ? std::getenv("CYBERWAVE_DATA_DIR") : "/tmp/cyberwave_data")
                    : std::move(base_dir)),
      ring_buffer_size_(ring_buffer_size), poll_interval_s_(poll_interval_s)
{
    std::filesystem::create_directories(base_dir_);
}

FilesystemDataBackend::~FilesystemDataBackend() { close(); }

void FilesystemDataBackend::publish(const std::string& channel, const DataBytes& payload,
                                    const nlohmann::json& metadata)
{
    const std::filesystem::path channel_dir = std::filesystem::path(base_dir_) / safe_channel_name(channel);
    std::filesystem::create_directories(channel_dir);

    const auto ts = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    const std::filesystem::path sample_path = channel_dir / (std::to_string(ts) + ".bin");
    const std::filesystem::path latest_path = channel_dir / "latest.bin";
    const std::filesystem::path tmp_sample = channel_dir / (".sample_" + std::to_string(ts) + ".tmp");
    const std::filesystem::path tmp_latest = channel_dir / (".latest_" + std::to_string(ts) + ".tmp");

    write_file_bytes(tmp_sample, payload);
    std::filesystem::rename(tmp_sample, sample_path);
    write_file_bytes(tmp_latest, payload);
    std::filesystem::rename(tmp_latest, latest_path);

    if (metadata.is_object() && !metadata.empty())
    {
        std::ofstream metadata_file(channel_dir / (std::to_string(ts) + ".meta.json"));
        if (!metadata_file)
            throw CyberwaveError("Failed to open metadata file for channel " + channel);
        metadata_file << metadata.dump();
    }

    std::vector<std::filesystem::path> bins;
    for (const auto& entry : std::filesystem::directory_iterator(channel_dir))
    {
        if (entry.is_regular_file() && is_numeric_sample_file(entry.path()))
            bins.push_back(entry.path());
    }
    std::sort(bins.begin(), bins.end());
    while (bins.size() > ring_buffer_size_)
    {
        const auto oldest = bins.front();
        bins.erase(bins.begin());
        std::filesystem::remove(oldest);
        std::filesystem::remove(oldest.parent_path() / (oldest.stem().string() + ".meta.json"));
    }
}

std::shared_ptr<DataSubscription> FilesystemDataBackend::subscribe(const std::string& channel, Callback callback,
                                                                   const std::string& policy)
{
    DataBackend::validate_policy(policy);
    const std::filesystem::path channel_dir = std::filesystem::path(base_dir_) / safe_channel_name(channel);
    std::filesystem::create_directories(channel_dir);

    auto watcher =
        std::make_shared<FilesystemWatcher>(channel, channel_dir, std::move(callback), poll_interval_s_, policy);
    watcher->start();
    {
        std::lock_guard<std::mutex> lock(watchers_mutex_);
        watchers_.push_back(std::make_shared<Watcher>(Watcher{watcher}));
    }
    return std::make_shared<FilesystemSubscriptionImpl>(watcher);
}

std::optional<DataSample> FilesystemDataBackend::latest(const std::string& channel, double)
{
    const std::filesystem::path latest_path =
        std::filesystem::path(base_dir_) / safe_channel_name(channel) / "latest.bin";
    if (!std::filesystem::exists(latest_path))
        return std::nullopt;

    DataSample sample;
    sample.channel = channel;
    sample.payload = read_file_bytes(latest_path);
    sample.timestamp = now_seconds();
    return sample;
}

void FilesystemDataBackend::close()
{
    std::vector<std::shared_ptr<Watcher>> watchers;
    {
        std::lock_guard<std::mutex> lock(watchers_mutex_);
        if (closed_)
            return;
        closed_ = true;
        watchers.swap(watchers_);
    }

    for (const auto& watcher : watchers)
    {
        if (watcher && watcher->impl)
            watcher->impl->stop();
    }
}

std::string build_data_key(const std::string& twin_uuid, const std::string& channel,
                           std::optional<std::string> sensor_name, const std::string& prefix)
{
    validate_data_key_parts(twin_uuid, channel, sensor_name, prefix);
    std::string key = prefix + "/" + twin_uuid + "/data/" + channel;
    if (sensor_name)
        key += "/" + *sensor_name;
    return key;
}

bool is_valid_data_key(const std::string& key)
{
    std::vector<std::string> segments;
    std::string current;
    for (char ch : key)
    {
        if (ch == '/')
        {
            segments.push_back(current);
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }
    segments.push_back(current);

    if (segments.size() != 4 && segments.size() != 5)
        return false;
    if (segments[2] != "data")
        return false;

    try
    {
        validate_data_key_parts(segments[1], segments[3],
                                segments.size() == 5 ? std::optional<std::string>(segments[4]) : std::nullopt,
                                segments[0]);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

DataBus::DataBus(std::shared_ptr<DataBackend> backend, std::string twin_uuid, std::optional<std::string> sensor_name,
                 std::string key_prefix)
    : backend_(std::move(backend)), twin_uuid_(std::move(twin_uuid)), sensor_name_(std::move(sensor_name)),
      key_prefix_(std::move(key_prefix))
{
    if (!backend_)
        throw CyberwaveError("DataBus requires a backend");
    validate_data_key_parts(twin_uuid_, "frames", sensor_name_, key_prefix_);
}

void DataBus::publish(const std::string& channel, const nlohmann::json& sample, const nlohmann::json& metadata)
{
    const std::string payload = sample.dump(-1, ' ', false, nlohmann::json::error_handler_t::strict);
    auto tmpl = get_or_create_template(channel, DATA_CONTENT_TYPE_JSON, metadata);
    backend_->publish(key_for_channel(channel), tmpl->pack(DataBytes(payload.begin(), payload.end())));
}

void DataBus::publish_bytes(const std::string& channel, const DataBytes& sample, const nlohmann::json& metadata)
{
    auto tmpl = get_or_create_template(channel, DATA_CONTENT_TYPE_BYTES, metadata);
    backend_->publish(key_for_channel(channel), tmpl->pack(sample));
}

void DataBus::publish_bytes(const std::string& channel, const std::string& sample, const nlohmann::json& metadata)
{
    publish_bytes(channel, DataBytes(sample.begin(), sample.end()), metadata);
}

void DataBus::publish_ndarray(const std::string& channel, const DataBytes& sample, std::vector<std::size_t> shape,
                              std::string dtype, const nlohmann::json& metadata)
{
    auto tmpl = get_or_create_template(channel, DATA_CONTENT_TYPE_NUMPY, metadata, std::move(shape), std::move(dtype));
    backend_->publish(key_for_channel(channel), tmpl->pack(sample));
}

std::shared_ptr<DataSubscription> DataBus::subscribe(const std::string& channel, Callback callback,
                                                     const std::string& policy)
{
    return subscribe_raw(
        channel,
        [callback = std::move(callback), this](const DataSample& sample)
        {
            try
            {
                callback(decode_value(sample.payload));
            }
            catch (...)
            {
            }
        },
        policy);
}

std::shared_ptr<DataSubscription> DataBus::subscribe_raw(const std::string& channel, DataBackend::Callback callback,
                                                         const std::string& policy)
{
    return backend_->subscribe(key_for_channel(channel), std::move(callback), policy);
}

std::optional<DataValue> DataBus::latest(const std::string& channel, double timeout_s,
                                         std::optional<double> max_age_ms) const
{
    if (max_age_ms && *max_age_ms < 0.0)
        throw std::invalid_argument("max_age_ms must be >= 0");

    auto sample = backend_->latest(key_for_channel(channel), timeout_s);
    if (!sample)
        return std::nullopt;

    const auto decoded = decode_data_sample(sample->payload);
    if (max_age_ms)
    {
        const double age_seconds = now_seconds() - decoded.first.ts;
        if (age_seconds > (*max_age_ms / 1000.0))
            return std::nullopt;
    }
    return decode_value(sample->payload);
}

std::optional<DataSample> DataBus::latest_raw(const std::string& channel, double timeout_s) const
{
    return backend_->latest(key_for_channel(channel), timeout_s);
}

void DataBus::close() const
{
    if (backend_)
        backend_->close();
}

std::shared_ptr<DataHeaderTemplate> DataBus::get_or_create_template(const std::string& channel,
                                                                    const std::string& content_type,
                                                                    const nlohmann::json& metadata,
                                                                    std::optional<std::vector<std::size_t>> shape,
                                                                    std::optional<std::string> dtype)
{
    std::lock_guard<std::mutex> lock(templates_mutex_);
    const auto it = templates_.find(channel);
    if (it != templates_.end())
    {
        const bool content_type_matches = it->second->content_type() == content_type;
        const bool shape_matches = it->second->shape() == shape;
        const bool dtype_matches = it->second->dtype() == dtype;
        if (content_type_matches && shape_matches && dtype_matches)
            return it->second;
    }

    auto tmpl = std::make_shared<DataHeaderTemplate>(content_type, std::move(shape), std::move(dtype), metadata);
    templates_[channel] = tmpl;
    return tmpl;
}

DataValue DataBus::decode_value(const DataBytes& wire_bytes) const
{
    const auto decoded = decode_data_sample(wire_bytes);
    const DataHeaderMeta& header = decoded.first;
    const DataBytes& payload = decoded.second;

    if (header.content_type == DATA_CONTENT_TYPE_JSON)
    {
        try
        {
            if (payload.empty())
                return nlohmann::json::object();
            return nlohmann::json::parse(payload.begin(), payload.end());
        }
        catch (const nlohmann::json::exception& e)
        {
            throw CyberwaveError(std::string("Invalid JSON data sample: ") + e.what());
        }
    }
    if (header.content_type == DATA_CONTENT_TYPE_NUMPY)
    {
        if (!header.shape || !header.dtype)
            throw CyberwaveError("numpy/ndarray sample missing shape or dtype");
        return NdarraySample{payload, *header.shape, *header.dtype};
    }
    return payload;
}

std::string DataBus::key_for_channel(const std::string& channel) const
{
    return build_data_key(twin_uuid_, channel, sensor_name_, key_prefix_);
}

} // namespace cyberwave
