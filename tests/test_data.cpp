#include "cyberwave/client.h"
#include "cyberwave/config.h"
#include "cyberwave/data.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>

using namespace cyberwave;

namespace
{

const std::string kTwinUuid = "550e8400-e29b-41d4-a716-446655440000";

std::string make_temp_dir()
{
    const auto now = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now())
                         .time_since_epoch()
                         .count();
    const auto path = std::filesystem::temp_directory_path() / ("cyberwave-data-test-" + std::to_string(now));
    std::filesystem::create_directories(path);
    return path.string();
}

void test_key_builder()
{
    const auto key = build_data_key(kTwinUuid, "frames", "default");
    assert(key == "cw/550e8400-e29b-41d4-a716-446655440000/data/frames/default");
    assert(is_valid_data_key(key));
    assert(!is_valid_data_key("cw/not-a-uuid/data/frames/default"));
}

void test_header_roundtrip()
{
    DataHeaderMeta header;
    header.content_type = DATA_CONTENT_TYPE_NUMPY;
    header.ts = 123.456;
    header.seq = 7;
    header.shape = std::vector<std::size_t>{2, 2};
    header.dtype = "uint8";
    header.metadata = nlohmann::json{{"sensor", "default"}};

    const DataBytes payload = {1, 2, 3, 4};
    const auto encoded = encode_data_sample(header, payload);
    const auto decoded = decode_data_sample(encoded);

    assert(decoded.first.content_type == DATA_CONTENT_TYPE_NUMPY);
    assert(decoded.first.seq == 7);
    assert(decoded.first.shape == std::optional<std::vector<std::size_t>>({2, 2}));
    assert(decoded.first.dtype == std::optional<std::string>("uint8"));
    assert(decoded.first.metadata.at("sensor") == "default");
    assert(decoded.second == payload);
}

void test_data_bus_roundtrips()
{
    auto backend = std::make_shared<FilesystemDataBackend>(make_temp_dir(), 100, 0.02);
    DataBus bus(backend, kTwinUuid);

    const nlohmann::json json_payload = {{"joint1", 0.5}, {"joint2", -1.0}};
    bus.publish("joint_states", json_payload);
    const auto json_value = bus.latest("joint_states");
    assert(json_value);
    assert(std::holds_alternative<nlohmann::json>(*json_value));
    assert(std::get<nlohmann::json>(*json_value) == json_payload);

    const DataBytes raw_payload = {0xAA, 0xBB, 0xCC};
    bus.publish_bytes("raw_bytes", raw_payload);
    const auto raw_value = bus.latest("raw_bytes");
    assert(raw_value);
    assert(std::holds_alternative<DataBytes>(*raw_value));
    assert(std::get<DataBytes>(*raw_value) == raw_payload);

    const DataBytes ndarray_payload = {1, 2, 3, 4};
    bus.publish_ndarray("frames", ndarray_payload, {2, 2}, "uint8");
    const auto ndarray_value = bus.latest("frames");
    assert(ndarray_value);
    assert(std::holds_alternative<NdarraySample>(*ndarray_value));
    const auto& ndarray = std::get<NdarraySample>(*ndarray_value);
    assert(ndarray.payload == ndarray_payload);
    assert((ndarray.shape == std::vector<std::size_t>{2, 2}));
    assert(ndarray.dtype == "uint8");

    const auto raw_sample = bus.latest_raw("raw_bytes");
    assert(raw_sample);
    assert(!raw_sample->payload.empty());

    backend->close();
    std::filesystem::remove_all(backend->base_dir());
}

void test_staleness_and_templates()
{
    auto backend = std::make_shared<FilesystemDataBackend>(make_temp_dir(), 100, 0.02);
    DataBus bus(backend, kTwinUuid);

    bus.publish_bytes("stale", DataBytes{1});
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto stale_value = bus.latest("stale", 1.0, 1.0);
    assert(!stale_value.has_value());

    bus.publish_bytes("seq_test", DataBytes{1});
    bus.publish_bytes("seq_test", DataBytes{2});
    bus.publish_bytes("seq_test", DataBytes{3});

    backend->close();
    std::filesystem::remove_all(backend->base_dir());
}

void test_subscribe()
{
    auto backend = std::make_shared<FilesystemDataBackend>(make_temp_dir(), 100, 0.02);
    DataBus bus(backend, kTwinUuid);

    std::mutex mutex;
    std::condition_variable cv;
    bool got_json = false;
    bool got_raw = false;

    auto json_sub = bus.subscribe(
        "telemetry",
        [&](const DataValue& value)
        {
            std::lock_guard<std::mutex> lock(mutex);
            got_json =
                std::holds_alternative<nlohmann::json>(value) && std::get<nlohmann::json>(value).at("battery") == 90;
            cv.notify_all();
        },
        "latest");

    auto raw_sub = bus.subscribe_raw(
        "telemetry_raw",
        [&](const DataSample& sample)
        {
            std::lock_guard<std::mutex> lock(mutex);
            got_raw = !sample.payload.empty();
            cv.notify_all();
        },
        "latest");

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    bus.publish("telemetry", nlohmann::json{{"battery", 90}});
    bus.publish_bytes("telemetry_raw", DataBytes{9, 8, 7});

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, std::chrono::seconds(2), [&]() { return got_json && got_raw; });
    assert(got_json);
    assert(got_raw);

    json_sub->close();
    raw_sub->close();
    backend->close();
    std::filesystem::remove_all(backend->base_dir());
}

void test_client_data()
{
    Config cfg;
    cfg.twin_uuid = kTwinUuid;
    cfg.api_key = "";
    Client client(cfg);

    auto backend = std::make_shared<FilesystemDataBackend>(make_temp_dir(), 100, 0.02);
    auto bus = client.data(backend);
    bus.publish("status", nlohmann::json{{"ok", true}});
    const auto value = bus.latest("status");
    assert(value);
    assert(std::holds_alternative<nlohmann::json>(*value));
    assert(std::get<nlohmann::json>(*value).at("ok") == true);

    backend->close();
    std::filesystem::remove_all(backend->base_dir());
}

} // namespace

int main()
{
    test_key_builder();
    test_header_roundtrip();
    test_data_bus_roundtrips();
    test_staleness_and_templates();
    test_subscribe();
    test_client_data();
    return 0;
}
