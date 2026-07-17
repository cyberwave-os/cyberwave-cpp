/**
 * @brief RGB and depth streaming abstractions built on top of `IMqttClient`.
 */

#ifndef CYBERWAVE_CAMERA_STREAMING_H
#define CYBERWAVE_CAMERA_STREAMING_H

#include "cyberwave/mqtt_interface.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cyberwave
{
// Internal: WebRTC producer adapter used by CameraStreamer (implemented in .cpp).
class WebRTCAdapter;

// Internal: JPEG -> H264 encoder used by CameraStreamer (implemented in .cpp).
struct JpegToH264EncoderImpl;

enum class PixelFormat : std::uint8_t
{
    RGB24 = 0,
    BGR24 = 1,
};

struct VideoFrame
{
    std::vector<std::uint8_t> data;
    // Optional: if provided, used for legacy MQTT /video JPEG fallback path.
    std::vector<std::uint8_t> jpeg_fallback;
    int width = 0;
    int height = 0;
    PixelFormat pixel_format = PixelFormat::BGR24;
    double timestamp = 0.0;
};

/**
 * @brief Abstract source of raw video frames for streaming.
 */
struct IFrameSource
{
    virtual ~IFrameSource() = default;

    /** Get next raw frame. Returns false if no frame (e.g. device closed). */
    virtual bool next_frame(VideoFrame& frame_out) = 0;
};

/**
 * @brief Streamer that publishes RGB frames to a twin video topic.
 */
class CameraStreamer
{
public:
    /**
     * @param mqtt Must outlive the streamer; can be nullptr (start() no-op).
     * @param twin_uuid Twin to stream to.
     * @param source Frame source; must outlive the streamer or be kept alive externally.
     * @param fps Target frames per second.
     */
    CameraStreamer(std::shared_ptr<IMqttClient> mqtt, const std::string& twin_uuid,
                   std::shared_ptr<IFrameSource> source, int fps = 30, std::string sensor_name = "",
                   bool enable_webrtc = false, bool enable_mqtt_video_fallback = true,
                   std::string webrtc_stun_url = "stun:stun.l.google.com:19302",
                   std::vector<std::string> webrtc_turn_servers = {});
    ~CameraStreamer();

    CameraStreamer(const CameraStreamer&) = delete;
    CameraStreamer& operator=(const CameraStreamer&) = delete;

    void start();
    void stop();
    bool running() const noexcept { return running_.load(); }

    /**
     * @brief Inject a log callback so WebRTC state messages use the caller's logger.
     *
     * Call before start(). If not set, the streamer uses its own default logger
     * which may have a different format from the rest of the application.
     */
    void set_log_callback(std::function<void(const std::string&)> fn);

    /**
     * @brief Enable or disable recording in the WebRTC offer sent to the media service.
     *
     * Call before start(). When true, the media service SFU will record the stream
     * and produce MP4/Parquet artifacts. Default is false (matching the Python SDK's
     * explicit _should_record pattern).
     */
    void set_recording(bool recording) { recording_ = recording; }

private:
    void stream_loop();
    void publish_edge_health(double now_seconds);

    std::shared_ptr<IMqttClient> mqtt_;
    std::string twin_uuid_;
    std::shared_ptr<IFrameSource> source_;
    int fps_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    // WebRTC producer options.
    std::string sensor_name_{};
    bool enable_webrtc_{false};
    bool enable_mqtt_video_fallback_{true};
    bool recording_{false};
    std::string webrtc_stun_url_;
    std::vector<std::string> webrtc_turn_servers_;
    std::unique_ptr<WebRTCAdapter> webrtc_adapter_;

    // Optional: JPEG -> Annex-B H264 conversion when WebRTC is enabled.
    std::unique_ptr<JpegToH264EncoderImpl> h264_encoder_;
    std::uint64_t frame_counter_{0};

    // MQTT fallback rate-limiting: cap at ~5fps to avoid flooding the broker.
    static constexpr double mqtt_fallback_min_interval_s_{0.2};
    double mqtt_fallback_last_send_ts_{0.0};

    // Stream health telemetry published by the streamer itself.
    double edge_health_stream_started_at_seconds_{0.0};
    double edge_health_last_publish_ts_seconds_{0.0};
    double edge_health_last_frame_ts_seconds_{0.0};
    std::uint64_t edge_health_frames_sent_{0};

    // Optional logger injected by the caller for consistent log formatting.
    std::function<void(const std::string&)> log_fn_;
};

/**
 * @brief Synthetic frame source for tests and placeholder streaming.
 */
class VirtualFrameSource : public IFrameSource
{
public:
    bool next_frame(VideoFrame& frame_out) override;
};

/** @brief Raw depth frame stored as row-major 16-bit millimetre values. */
struct DepthFrame
{
    std::vector<uint16_t> data; // width * height elements
    int width = 0;
    int height = 0;
    double timestamp = 0.0;
};

/** @brief One coloured point-cloud point as `{x, y, z, r, g, b}`. */
using PointXYZRGB = std::array<float, 6>;

/**
 * @brief Abstract source of depth frames and optional point clouds.
 */
struct IDepthSource
{
    virtual ~IDepthSource() = default;

    /** Get next depth frame. Returns false if unavailable. */
    virtual bool next_depth_frame(DepthFrame& frame_out) = 0;

    /** Optionally get a point cloud. Default: not supported (returns false). */
    virtual bool next_point_cloud(std::vector<PointXYZRGB>& /*cloud_out*/) { return false; }
};

/**
 * @brief Streamer that publishes depth and optional point-cloud payloads.
 *
 * Pointcloud publishes run on a dedicated background thread with a
 * single-slot "latest wins" buffer so that the acquisition loop is never
 * blocked by MQTT back-pressure.  If the network cannot keep up, stale
 * frames are silently replaced by newer ones — the correct behaviour for
 * real-time sensor streams.
 */
class DepthStreamer
{
public:
    DepthStreamer(std::shared_ptr<IMqttClient> mqtt, const std::string& twin_uuid, std::shared_ptr<IDepthSource> source,
                  int fps = 10, bool publish_depth = true, bool publish_pointcloud = true);
    ~DepthStreamer();

    DepthStreamer(const DepthStreamer&) = delete;
    DepthStreamer& operator=(const DepthStreamer&) = delete;

    void start();
    void stop();
    bool running() const noexcept { return running_.load(); }

private:
    void stream_loop();
    void pointcloud_publish_loop();

    std::shared_ptr<IMqttClient> mqtt_;
    std::string twin_uuid_;
    std::shared_ptr<IDepthSource> source_;
    int fps_;
    bool publish_depth_{true};
    bool publish_pointcloud_{true};
    std::atomic<bool> running_{false};
    std::thread thread_;

    // Single-slot latest-wins buffer for async pointcloud publishing.
    std::thread pc_publish_thread_;
    std::mutex pc_mutex_;
    std::condition_variable pc_cv_;
    std::string pc_pending_topic_;
    std::string pc_pending_payload_;
    bool pc_has_pending_{false};
};

/**
 * Virtual depth source: returns a 4x4 placeholder depth frame (no real sensor).
 * For tests and environments without depth hardware.
 */
class VirtualDepthSource : public IDepthSource
{
public:
    bool next_depth_frame(DepthFrame& frame_out) override;
};

} // namespace cyberwave

#endif // CYBERWAVE_CAMERA_STREAMING_H
