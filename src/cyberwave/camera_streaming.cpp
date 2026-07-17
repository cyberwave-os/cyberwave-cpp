#include "cyberwave/camera_streaming.h"

#include <cpprest/details/basic_types.h>
#include <cpprest/json.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <nlohmann/json.hpp>

#ifndef CYBERWAVE_HAS_LIBDATACHANNEL
#define CYBERWAVE_HAS_LIBDATACHANNEL 0
#endif

#ifndef CYBERWAVE_HAS_FFMPEG
#define CYBERWAVE_HAS_FFMPEG 0
#endif

#if CYBERWAVE_HAS_LIBDATACHANNEL
#include <rtc/rtc.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#endif

// Important: these includes must live at file scope (not inside namespace cyberwave),
// otherwise C++ headers will get declared as cyberwave::std (breaking standard library types).
#if CYBERWAVE_HAS_FFMPEG
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}
#endif

namespace cyberwave
{

namespace
{

// Smallest valid 1x1 pixel grayscale JPEG (119 bytes) for fallback tests.
static const unsigned char MINIMAL_JPEG[] = {
    0xff, 0xd8, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 0xc2, 0x00, 0x0b, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x11,
    0x00, 0xff, 0xc4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x3f, 0xff, 0xd9,
};
static const size_t MINIMAL_JPEG_SIZE = sizeof(MINIMAL_JPEG);

static const char BASE64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const unsigned char* data, size_t len)
{
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3)
    {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < len)
            n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < len)
            n |= static_cast<unsigned int>(data[i + 2]);
        out.push_back(BASE64_CHARS[(n >> 18) & 63]);
        out.push_back(BASE64_CHARS[(n >> 12) & 63]);
        out.push_back(i + 1 < len ? BASE64_CHARS[(n >> 6) & 63] : '=');
        out.push_back(i + 2 < len ? BASE64_CHARS[n & 63] : '=');
    }
    return out;
}

utility::string_t from_std(const std::string& s) { return utility::conversions::to_string_t(s); }

std::string json_serialize(const web::json::value& v)
{
    std::ostringstream os;
    v.serialize(os);
    return os.str();
}

double timestamp_now()
{
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

constexpr double edge_health_publish_interval_seconds = 5.0;
constexpr double edge_health_stale_timeout_seconds = 60.0;

} // namespace

// ============================================================================
// WebRTC producer (libdatachannel) used by camera streamers
// ============================================================================

using Json = nlohmann::json;

namespace
{

struct CameraStreamEdgeHealthState
{
    double stream_started_at_seconds{0.0};
    double last_frame_ts_seconds{0.0};
    std::uint64_t frames_sent{0};
    std::string sensor_name;
};

void publish_camera_stream_edge_health(IMqttClient& mqtt, const std::string& twin_uuid, const double now_seconds,
                                       const CameraStreamEdgeHealthState& state)
{
    if (!mqtt.is_connected() || twin_uuid.empty() || state.stream_started_at_seconds <= 0.0)
    {
        return;
    }

    const double uptime_seconds = std::max(0.0, now_seconds - state.stream_started_at_seconds);
    const double fps = (uptime_seconds > 0.0) ? (static_cast<double>(state.frames_sent) / uptime_seconds) : 0.0;
    const double time_since_last_frame =
        (state.last_frame_ts_seconds > 0.0) ? (now_seconds - state.last_frame_ts_seconds) : 0.0;
    const bool is_stale =
        state.last_frame_ts_seconds <= 0.0 || time_since_last_frame > edge_health_stale_timeout_seconds;
    const bool connected = !is_stale;
    const std::string camera_id = state.sensor_name.empty() ? "stream" : state.sensor_name;
    const double rounded_uptime = std::round(uptime_seconds * 10.0) / 10.0;
    const double rounded_fps = std::round(fps * 100.0) / 100.0;

    Json message = {{"type", "edge_health"},
                    {"timestamp", now_seconds},
                    {"edge_id", twin_uuid},
                    {"twin_uuid", twin_uuid},
                    {"uptime_seconds", rounded_uptime},
                    {"stream_count", 1},
                    {"healthy_streams", connected ? 1 : 0},
                    {"camera_config", nullptr},
                    {"streams",
                     {{"stream",
                       {{"camera_id", camera_id},
                        {"connection_state", connected ? "connected" : "disconnected"},
                        {"ice_connection_state", connected ? "connected" : "new"},
                        {"frames_sent", state.frames_sent},
                        {"last_frame_ts", state.last_frame_ts_seconds},
                        {"fps", rounded_fps},
                        {"uptime_seconds", rounded_uptime},
                        {"restart_count", 0},
                        {"is_stale", is_stale},
                        {"is_healthy", !is_stale}}}}}};

    const std::string topic = mqtt.get_topic_prefix() + "cyberwave/twin/" + twin_uuid + "/edge_health";
    try
    {
        mqtt.publish(topic, message.dump());
    }
    catch (...)
    {
        // Best-effort health telemetry.
    }
}

} // namespace

class WebRTCAdapter
{
public:
    using PublishSignalCallback = std::function<void(const Json&)>;

    using LogCallback = std::function<void(const std::string&)>;

    struct Config
    {
        std::string sensor{"default"};
        std::string stun_url{"stun:stun.l.google.com:19302"};
        std::vector<std::string> turn_servers;
        bool recording{false};
        /// Optional logger for informational WebRTC state messages.
        LogCallback log_fn;
    };

    virtual ~WebRTCAdapter() = default;

    virtual void start(const Config& cfg, PublishSignalCallback publish_signal) = 0;
    virtual void stop() = 0;

    virtual bool handle_answer(const Json& answer) = 0;
    virtual bool handle_candidate(const Json& candidate_message) = 0;

    // Frames should be Annex-B H264 bytes.
    virtual bool send_frame(const std::vector<std::uint8_t>& frame_annexb, std::uint64_t timestamp_us) = 0;
};

namespace
{
[[maybe_unused]] std::vector<std::uint8_t> as_annex_b_h264_fallback(const std::vector<unsigned char>& bytes)
{
    // Not a real encoder. This exists to validate the WebRTC plumbing while
    // we wire proper H264 encoding in a follow-up pass.
    std::vector<std::uint8_t> payload;
    payload.reserve(bytes.size() + 4);
    payload.push_back(0x00);
    payload.push_back(0x00);
    payload.push_back(0x00);
    payload.push_back(0x01);
    payload.insert(payload.end(), bytes.begin(), bytes.end());
    return payload;
}
} // namespace

// ============================================================================
// JPEG -> Annex-B H264 encoder for WebRTC
// ============================================================================

#if CYBERWAVE_HAS_FFMPEG

namespace
{

bool has_annexb_start_code(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size < 3)
    {
        return false;
    }
    for (std::size_t i = 0; i + 2 < size; ++i)
    {
        if (data[i] == 0x00 && data[i + 1] == 0x00 &&
            ((i + 3 < size && data[i + 2] == 0x00 && data[i + 3] == 0x01) || data[i + 2] == 0x01))
        {
            return true;
        }
    }
    return false;
}

std::size_t annexb_start_code_size_at(const std::vector<std::uint8_t>& bytes, std::size_t index)
{
    if (index + 3 < bytes.size() && bytes[index] == 0x00 && bytes[index + 1] == 0x00 && bytes[index + 2] == 0x00 &&
        bytes[index + 3] == 0x01)
    {
        return 4;
    }
    if (index + 2 < bytes.size() && bytes[index] == 0x00 && bytes[index + 1] == 0x00 && bytes[index + 2] == 0x01)
    {
        return 3;
    }
    return 0;
}

std::vector<std::uint8_t> normalize_annexb_to_long_start_codes(const std::vector<std::uint8_t>& bytes)
{
    std::vector<std::uint8_t> out;
    if (bytes.empty())
    {
        return out;
    }

    std::size_t i = 0;
    while (i < bytes.size())
    {
        const std::size_t start_code_size = annexb_start_code_size_at(bytes, i);
        if (start_code_size == 0)
        {
            ++i;
            continue;
        }

        const std::size_t nalu_start = i + start_code_size;
        std::size_t nalu_end = bytes.size();

        std::size_t j = nalu_start;
        while (j < bytes.size())
        {
            const std::size_t next_start_code_size = annexb_start_code_size_at(bytes, j);
            if (next_start_code_size > 0)
            {
                nalu_end = j;
                break;
            }
            ++j;
        }

        if (nalu_start < nalu_end)
        {
            out.insert(out.end(), {0x00, 0x00, 0x00, 0x01});
            out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(nalu_start),
                       bytes.begin() + static_cast<std::ptrdiff_t>(nalu_end));
        }

        i = nalu_end;
    }
    return out;
}

std::vector<std::uint8_t> avcc_packet_to_annexb(const std::uint8_t* data, std::size_t size)
{
    std::vector<std::uint8_t> out;
    std::size_t offset = 0;
    while (offset + 4 <= size)
    {
        const std::uint32_t nalu_size =
            (static_cast<std::uint32_t>(data[offset]) << 24) | (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(data[offset + 2]) << 8) | (static_cast<std::uint32_t>(data[offset + 3]));
        offset += 4;
        if (nalu_size == 0 || offset + nalu_size > size)
        {
            out.clear();
            return out;
        }
        out.push_back(0x00);
        out.push_back(0x00);
        out.push_back(0x00);
        out.push_back(0x01);
        out.insert(out.end(), data + offset, data + offset + nalu_size);
        offset += nalu_size;
    }
    if (offset != size)
    {
        out.clear();
    }
    return out;
}

std::vector<std::uint8_t> extract_parameter_sets_annexb(const std::vector<std::uint8_t>& bytes)
{
    std::vector<std::uint8_t> out;
    bool have_sps = false;
    bool have_pps = false;
    if (bytes.size() < 5)
    {
        return out;
    }

    std::size_t i = 0;
    while (i < bytes.size())
    {
        const std::size_t start_code_size = annexb_start_code_size_at(bytes, i);
        if (start_code_size == 0)
        {
            ++i;
            continue;
        }

        const std::size_t nalu_start = i + start_code_size;
        std::size_t nalu_end = bytes.size();
        std::size_t j = nalu_start;
        while (j < bytes.size())
        {
            const std::size_t next_start_code_size = annexb_start_code_size_at(bytes, j);
            if (next_start_code_size > 0)
            {
                nalu_end = j;
                break;
            }
            ++j;
        }

        if (nalu_start < nalu_end)
        {
            const std::uint8_t nal_type = bytes[nalu_start] & 0x1F;
            if (nal_type == 7 || nal_type == 8)
            {
                out.insert(out.end(), {0x00, 0x00, 0x00, 0x01});
                out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(nalu_start),
                           bytes.begin() + static_cast<std::ptrdiff_t>(nalu_end));
                have_sps = have_sps || (nal_type == 7);
                have_pps = have_pps || (nal_type == 8);
            }
        }

        i = nalu_end;
    }

    if (!(have_sps && have_pps))
    {
        out.clear();
    }
    return out;
}

bool has_annexb_parameter_sets(const std::vector<std::uint8_t>& bytes)
{
    const auto params = extract_parameter_sets_annexb(bytes);
    return !params.empty();
}

bool has_annexb_idr(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() < 5)
    {
        return false;
    }
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        const std::size_t start_code_size = annexb_start_code_size_at(bytes, i);
        if (start_code_size > 0)
        {
            const std::size_t nalu_start = i + start_code_size;
            if (nalu_start < bytes.size() && (bytes[nalu_start] & 0x1F) == 5)
            {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::uint8_t> convert_extradata_to_annexb(const AVCodecContext* codec_ctx)
{
    std::vector<std::uint8_t> out;
    if (codec_ctx == nullptr || codec_ctx->extradata == nullptr || codec_ctx->extradata_size <= 0)
    {
        return out;
    }
    const auto* data = reinterpret_cast<const std::uint8_t*>(codec_ctx->extradata);
    const std::size_t size = static_cast<std::size_t>(codec_ctx->extradata_size);

    // Already looks like Annex-B.
    if (size >= 4 && data[0] == 0x00 && data[1] == 0x00 && ((data[2] == 0x00 && data[3] == 0x01) || data[2] == 0x01))
    {
        out.insert(out.end(), data, data + size);
        return out;
    }

    // AVCDecoderConfigurationRecord
    if (size < 7 || data[0] != 1)
    {
        return out;
    }

    std::size_t offset = 5;
    const std::uint8_t num_sps = data[offset++] & 0x1F;
    for (std::uint8_t i = 0; i < num_sps; ++i)
    {
        if (offset + 2 > size)
        {
            return {};
        }
        const std::uint16_t sps_size = (static_cast<std::uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        if (offset + sps_size > size)
        {
            return {};
        }
        out.insert(out.end(), {0x00, 0x00, 0x00, 0x01});
        out.insert(out.end(), data + offset, data + offset + sps_size);
        offset += sps_size;
    }

    if (offset + 1 > size)
    {
        return out;
    }

    const std::uint8_t num_pps = data[offset++];
    for (std::uint8_t i = 0; i < num_pps; ++i)
    {
        if (offset + 2 > size)
        {
            return {};
        }
        const std::uint16_t pps_size = (static_cast<std::uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        if (offset + pps_size > size)
        {
            return {};
        }
        out.insert(out.end(), {0x00, 0x00, 0x00, 0x01});
        out.insert(out.end(), data + offset, data + offset + pps_size);
        offset += pps_size;
    }
    return out;
}

std::vector<std::string> preferred_h264_codecs(const std::string& requested)
{
    if (requested.empty() || requested == "auto")
    {
        return {"h264_nvenc", "h264_vaapi", "h264_v4l2m2m", "h264_qsv", "libx264", "h264"};
    }
    return {requested};
}

int get_env_positive_or_throw(const char* name, int default_value)
{
    const char* raw = std::getenv(name);
    if (raw == nullptr)
    {
        return default_value;
    }
    const int value = std::stoi(raw);
    if (value <= 0)
    {
        throw std::runtime_error(std::string(name) + " must be a positive integer");
    }
    return value;
}

std::string get_env_or_default(const char* name, const std::string& default_value)
{
    const char* raw = std::getenv(name);
    if (raw == nullptr)
    {
        return default_value;
    }
    return std::string(raw);
}

} // namespace

struct JpegToH264EncoderImpl
{
    explicit JpegToH264EncoderImpl(int fps) : fps_(fps > 0 ? fps : 30)
    {
        h264_encoder_name_ = get_env_or_default("CYBERWAVE_H264_ENCODER", "auto");
        h264_preset_ = get_env_or_default("CYBERWAVE_H264_PRESET", "veryfast");
        h264_bitrate_kbps_ = get_env_positive_or_throw("CYBERWAVE_H264_BITRATE_KBPS", 2500);
        h264_gop_size_ = get_env_positive_or_throw("CYBERWAVE_H264_GOP", std::max(1, fps_ * 2));
        {
            const char* raw = std::getenv("CYBERWAVE_H264_THREADS");
            h264_threads_ = (raw != nullptr) ? std::max(0, std::stoi(raw)) : 0;
        }
    }

    ~JpegToH264EncoderImpl() { cleanup(); }

    bool encode_to_annexb_h264(const VideoFrame& frame_in, std::vector<std::uint8_t>& out_annexb_h264)
    {
        out_annexb_h264.clear();
        if (frame_in.data.empty() || frame_in.width <= 0 || frame_in.height <= 0)
        {
            return false;
        }

        AVPixelFormat source_fmt = AV_PIX_FMT_NONE;
        switch (frame_in.pixel_format)
        {
            case PixelFormat::BGR24:
                source_fmt = AV_PIX_FMT_BGR24;
                break;
            case PixelFormat::RGB24:
                source_fmt = AV_PIX_FMT_RGB24;
                break;
            default:
                return false;
        }

        const std::size_t expected_size =
            static_cast<std::size_t>(frame_in.width) * static_cast<std::size_t>(frame_in.height) * 3U;
        if (frame_in.data.size() < expected_size)
        {
            return false;
        }

        if (!ensure_initialized(frame_in.width, frame_in.height, source_fmt))
        {
            return false;
        }

        AVCodecContext* codec_ctx = codec_ctx_;
        if (!codec_ctx_ || !frame_ || !packet_ || !sws_ctx_)
        {
            return false;
        }

        if (av_frame_make_writable(frame_) < 0)
        {
            return false;
        }

        const uint8_t* src_slice[1] = {reinterpret_cast<const uint8_t*>(frame_in.data.data())};
        const int src_stride[1] = {frame_in.width * 3};
        sws_scale(sws_ctx_, src_slice, src_stride, 0, codec_height_, frame_->data, frame_->linesize);

        frame_->pts = pts_++;
        frame_->pict_type = (frame_->pts % std::max(1, h264_gop_size_) == 0) ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_NONE;

        if (avcodec_send_frame(codec_ctx, frame_) < 0)
        {
            return false;
        }

        bool emitted_key_packet = false;
        while (true)
        {
            const int rc = avcodec_receive_packet(codec_ctx, packet_);
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
            {
                break;
            }
            if (rc < 0)
            {
                break;
            }

            emitted_key_packet = emitted_key_packet || ((packet_->flags & AV_PKT_FLAG_KEY) != 0);

            const auto* packet_data = reinterpret_cast<const std::uint8_t*>(packet_->data);
            const std::size_t packet_size = static_cast<std::size_t>(packet_->size);

            std::vector<std::uint8_t> packet_annexb;
            if (has_annexb_start_code(packet_data, packet_size))
            {
                packet_annexb.insert(packet_annexb.end(), packet_data, packet_data + packet_size);
                const auto normalized = normalize_annexb_to_long_start_codes(packet_annexb);
                if (!normalized.empty())
                {
                    packet_annexb = normalized;
                }
            }
            else
            {
                const auto converted = avcc_packet_to_annexb(packet_data, packet_size);
                if (!converted.empty())
                {
                    packet_annexb = converted;
                }
                else
                {
                    packet_annexb.insert(packet_annexb.end(), packet_data, packet_data + packet_size);
                }
            }

            const auto packet_parameter_sets = extract_parameter_sets_annexb(packet_annexb);
            if (!packet_parameter_sets.empty())
            {
                parameter_sets_annexb_ = packet_parameter_sets;
            }

            emitted_key_packet = emitted_key_packet || has_annexb_idr(packet_annexb);
            out_annexb_h264.insert(out_annexb_h264.end(), packet_annexb.begin(), packet_annexb.end());
            av_packet_unref(packet_);
        }

        if (out_annexb_h264.empty())
        {
            return false;
        }

        if (parameter_sets_annexb_.empty())
        {
            auto late_extradata = convert_extradata_to_annexb(codec_ctx_);
            if (!late_extradata.empty())
            {
                parameter_sets_annexb_ = std::move(late_extradata);
            }
        }

        if (emitted_key_packet && !parameter_sets_annexb_.empty() && !has_annexb_parameter_sets(out_annexb_h264))
        {
            out_annexb_h264.insert(out_annexb_h264.begin(), parameter_sets_annexb_.begin(),
                                   parameter_sets_annexb_.end());
        }

        return true;
    }

private:
    bool ensure_initialized(int width, int height, AVPixelFormat source_fmt)
    {
        // Keep encoder state in sync with the actual incoming frame size.
        if (codec_ctx_ != nullptr)
        {
            if (codec_ctx_->width == width && codec_ctx_->height == height)
            {
                return true;
            }
            cleanup();
        }

        codec_width_ = width;
        codec_height_ = height;

        for (const auto& codec_name : preferred_h264_codecs(h264_encoder_name_))
        {
            const AVCodec* codec = avcodec_find_encoder_by_name(codec_name.c_str());
            if (!codec)
            {
                continue;
            }

            codec_name_ = codec_name;
            codec_ctx_ = avcodec_alloc_context3(codec);
            if (!codec_ctx_)
            {
                continue;
            }

            codec_ctx_->width = codec_width_;
            codec_ctx_->height = codec_height_;
            codec_ctx_->time_base = AVRational{1, std::max(1, fps_)};
            codec_ctx_->framerate = AVRational{std::max(1, fps_), 1};
            codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
            codec_ctx_->bit_rate = static_cast<std::int64_t>(h264_bitrate_kbps_) * 1000LL;
            codec_ctx_->gop_size = std::max(1, h264_gop_size_);
            codec_ctx_->max_b_frames = 0;
            codec_ctx_->thread_count = h264_threads_; // 0 = auto; 1 = single-threaded (minimal CPU steal)

            if (codec_name_ == "libx264")
            {
                const std::string x264_params = "keyint=" + std::to_string(std::max(1, h264_gop_size_)) +
                                                ":min-keyint=" + std::to_string(std::max(1, h264_gop_size_)) +
                                                ":scenecut=0:repeat-headers=1:annexb=1";
                av_opt_set(codec_ctx_->priv_data, "preset", h264_preset_.c_str(), 0);
                av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);
                av_opt_set(codec_ctx_->priv_data, "profile", "baseline", 0);
                av_opt_set(codec_ctx_->priv_data, "x264-params", x264_params.c_str(), 0);
            }

            codec_ctx_->flags &= ~AV_CODEC_FLAG_GLOBAL_HEADER;

            if (avcodec_open2(codec_ctx_, codec, nullptr) < 0)
            {
                continue;
            }

            parameter_sets_annexb_ = convert_extradata_to_annexb(codec_ctx_);

            frame_ = av_frame_alloc();
            packet_ = av_packet_alloc();
            if (!frame_ || !packet_)
            {
                cleanup();
                continue;
            }

            frame_->format = codec_ctx_->pix_fmt;
            frame_->width = codec_ctx_->width;
            frame_->height = codec_ctx_->height;

            if (av_frame_get_buffer(frame_, 32) < 0)
            {
                cleanup();
                continue;
            }

            sws_ctx_ = sws_getContext(codec_width_, codec_height_, source_fmt, codec_width_, codec_height_,
                                      AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!sws_ctx_)
            {
                cleanup();
                continue;
            }

            return true;
        }

        return false;
    }

    void cleanup()
    {
        if (packet_ != nullptr)
        {
            av_packet_free(&packet_);
            packet_ = nullptr;
        }
        if (frame_ != nullptr)
        {
            av_frame_free(&frame_);
            frame_ = nullptr;
        }
        if (sws_ctx_ != nullptr)
        {
            sws_freeContext(sws_ctx_);
            sws_ctx_ = nullptr;
        }
        if (codec_ctx_ != nullptr)
        {
            avcodec_free_context(&codec_ctx_);
            codec_ctx_ = nullptr;
        }
        codec_name_.clear();
        parameter_sets_annexb_.clear();
        pts_ = 0;
        codec_width_ = 0;
        codec_height_ = 0;
    }

private:
    int fps_{30};

    std::string h264_encoder_name_;
    std::string h264_preset_;
    int h264_bitrate_kbps_{2500};
    int h264_gop_size_{60};
    int h264_threads_{0};

    int codec_width_{0};
    int codec_height_{0};
    std::string codec_name_;

    AVCodecContext* codec_ctx_{nullptr};
    SwsContext* sws_ctx_{nullptr};
    AVFrame* frame_{nullptr};
    AVPacket* packet_{nullptr};
    std::int64_t pts_{0};
    std::vector<std::uint8_t> parameter_sets_annexb_;
};

#else

// Buildable stub: keeps SDK compilable when FFmpeg/OpenCV dev headers aren't
// available. In that case, WebRTC video will fall back to MQTT.
struct JpegToH264EncoderImpl
{
    explicit JpegToH264EncoderImpl(int /*fps*/) {}
    bool encode_to_annexb_h264(const VideoFrame& /*frame_in*/, std::vector<std::uint8_t>& /*out_annexb_h264*/)
    {
        return false;
    }
};

#endif

// Only compile the real libdatachannel adapter when the rtc.hpp header exists.
#if CYBERWAVE_HAS_LIBDATACHANNEL

class LibDataChannelWebRTCAdapter final : public WebRTCAdapter
{
public:
    void start(const Config& cfg, PublishSignalCallback publish_signal) override
    {
        if (!publish_signal)
        {
            throw std::runtime_error("WebRTC publish_signal callback is required");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (started_)
        {
            return;
        }

        publish_signal_ = std::move(publish_signal);
        log_fn_ = cfg.log_fn;
        sensor_ = cfg.sensor;
        recording_ = cfg.recording;

        rtc::Configuration rtc_cfg;
        if (!cfg.stun_url.empty())
        {
            rtc_cfg.iceServers.emplace_back(cfg.stun_url);
        }
        for (const auto& turn : cfg.turn_servers)
        {
            rtc_cfg.iceServers.emplace_back(turn);
        }
        rtc_cfg.disableAutoNegotiation = true;

        peer_connection_ = std::make_shared<rtc::PeerConnection>(rtc_cfg);
        register_callbacks_locked();

        ssrc_ = 1 + (std::rand() % 0xFFFFFFFEu);
        auto video_description = rtc::Description::Video("video", rtc::Description::Direction::SendOnly);
        video_description.addH264Codec(payload_type_h264_,
                                       "profile-level-id=42001f;packetization-mode=1;level-asymmetry-allowed=1");
        video_description.addSSRC(ssrc_, cname_, msid_, track_id_);
        video_track_ = peer_connection_->addTrack(video_description);

        rtp_config_ =
            std::make_shared<rtc::RtpPacketizationConfig>(ssrc_, cname_, payload_type_h264_, video_clock_rate_);
        auto packetizer =
            std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::LongStartSequence, rtp_config_);
        auto sr_reporter = std::make_shared<rtc::RtcpSrReporter>(rtp_config_);
        auto nack_responder = std::make_shared<rtc::RtcpNackResponder>();
        packetizer->addToChain(sr_reporter);
        sr_reporter->addToChain(nack_responder);

        video_track_->setMediaHandler(packetizer);
        video_track_->onOpen([this]() {});

        log("[WebRTCAdapter] Calling setLocalDescription (stun={})", cfg.stun_url);
        peer_connection_->setLocalDescription();
        log("[WebRTCAdapter] setLocalDescription returned, gatheringState={}",
            static_cast<int>(peer_connection_->gatheringState()));
        started_ = true;
    }

    void stop() override
    {
        // Move peer_connection out of the lock scope before destroying it.
        // PeerConnection destruction fires onStateChange callbacks that
        // acquire mutex_, causing a deadlock if we hold it during reset().
        std::shared_ptr<rtc::PeerConnection> pc_to_destroy;
        std::shared_ptr<rtc::Track> track_to_destroy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            started_ = false;
            connected_ = false;
            remote_description_set_ = false;
            local_sdp_.reset();
            track_to_destroy = std::move(video_track_);
            pc_to_destroy = std::move(peer_connection_);
            rtp_config_.reset();
            ssrc_ = 0;
            publish_signal_ = nullptr;
        }
        // Destroy outside the lock to avoid deadlock with callbacks.
        track_to_destroy.reset();
        pc_to_destroy.reset();
    }

    bool handle_answer(const Json& answer) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!peer_connection_)
        {
            return false;
        }
        if (!answer.contains("sdp") || !answer["sdp"].is_string())
        {
            return false;
        }

        try
        {
            const std::string sdp = answer["sdp"].get<std::string>();
            const std::string type = (answer.contains("type") && answer["type"].is_string())
                                         ? answer["type"].get<std::string>()
                                         : std::string("answer");
            peer_connection_->setRemoteDescription(rtc::Description(sdp, type));
            remote_description_set_ = true;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool handle_candidate(const Json& candidate_message) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!peer_connection_)
        {
            return false;
        }
        if (!candidate_message.contains("candidate") || !candidate_message["candidate"].is_object())
        {
            return false;
        }

        try
        {
            const auto& candidate_obj = candidate_message["candidate"];
            if (!candidate_obj.contains("candidate") || !candidate_obj["candidate"].is_string())
            {
                return false;
            }
            const std::string candidate = candidate_obj["candidate"].get<std::string>();
            const std::string mid = (candidate_obj.contains("sdpMid") && candidate_obj["sdpMid"].is_string())
                                        ? candidate_obj["sdpMid"].get<std::string>()
                                        : std::string();
            peer_connection_->addRemoteCandidate(rtc::Candidate(candidate, mid));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool send_frame(const std::vector<std::uint8_t>& frame_annexb, std::uint64_t timestamp_us) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!video_track_)
        {
            return false;
        }
        if (!remote_description_set_)
        {
            return false;
        }
        if (!connected_)
        {
            return false;
        }

        try
        {
            const auto timestamp_90khz = static_cast<std::uint32_t>((timestamp_us * video_clock_rate_) / 1000000ULL);
            if (rtp_config_)
            {
                rtp_config_->timestamp = timestamp_90khz;
            }
            const auto* bytes = reinterpret_cast<const std::byte*>(frame_annexb.data());
            video_track_->send(bytes, frame_annexb.size());
            (void)connected_;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

private:
    void register_callbacks_locked()
    {
        peer_connection_->onStateChange(
            [this](rtc::PeerConnection::State state)
            {
                log("[WebRTCAdapter] onStateChange: state={}", static_cast<int>(state));
                std::lock_guard<std::mutex> lock(mutex_);
                connected_ = (state == rtc::PeerConnection::State::Connected);
            });

        peer_connection_->onGatheringStateChange(
            [this](rtc::PeerConnection::GatheringState state)
            {
                log("[WebRTCAdapter] onGatheringStateChange: state={}", static_cast<int>(state));
                if (state != rtc::PeerConnection::GatheringState::Complete)
                {
                    return;
                }
                std::lock_guard<std::mutex> lock(mutex_);
                const auto local_description = peer_connection_->localDescription();
                if (!local_description.has_value())
                {
                    return;
                }

                local_sdp_ = std::string(local_description.value());
                Json payload = {
                    {"target", "backend"},
                    {"sender", "edge"},
                    {"type", local_description->typeString()},
                    {"sdp", local_sdp_.value()},
                    {"timestamp", timestamp_now()},
                    {"recording", recording_},
                };
                if (!sensor_.empty())
                {
                    payload["sensor"] = sensor_;
                }
                publish_signal_(payload);
            });

        peer_connection_->onLocalCandidate(
            [this](const rtc::Candidate& candidate)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                Json payload = {
                    {"target", "backend"},
                    {"sender", "edge"},
                    {"type", "candidate"},
                    {"candidate",
                     {
                         {"candidate", std::string(candidate)},
                         {"sdpMid", candidate.mid()},
                     }},
                    {"timestamp", timestamp_now()},
                };
                if (!sensor_.empty())
                {
                    payload["sensor"] = sensor_;
                }
                publish_signal_(payload);
            });
    }

    mutable std::mutex mutex_;
    PublishSignalCallback publish_signal_;

    std::shared_ptr<rtc::PeerConnection> peer_connection_;
    std::shared_ptr<rtc::Track> video_track_;
    std::shared_ptr<rtc::RtpPacketizationConfig> rtp_config_;
    std::optional<std::string> local_sdp_;
    std::string sensor_{};
    bool recording_{false};
    LogCallback log_fn_;

    std::atomic<bool> started_{false};
    bool connected_{false};
    bool remote_description_set_{false};

    std::uint32_t ssrc_{0};
    static constexpr std::uint8_t payload_type_h264_{102};
    static constexpr std::uint32_t video_clock_rate_{90000};
    static constexpr const char* cname_{"cw-cyberwave-cpp"};
    static constexpr const char* msid_{"cw-cyberwave-cpp-stream"};
    static constexpr const char* track_id_{"cw-cyberwave-cpp-video"};

    template <typename... Args>
    void log(fmt::format_string<Args...> fmt_str, Args&&... args) const
    {
        auto msg = fmt::format(fmt_str, std::forward<Args>(args)...);
        if (log_fn_)
        {
            log_fn_(msg);
        }
        else
        {
            spdlog::info("{}", msg);
        }
    }
};

std::unique_ptr<WebRTCAdapter> create_webrtc_adapter() { return std::make_unique<LibDataChannelWebRTCAdapter>(); }

#else

std::unique_ptr<WebRTCAdapter> create_webrtc_adapter() { return nullptr; }

#endif

// --- VirtualFrameSource ---
bool VirtualFrameSource::next_frame(VideoFrame& frame_out)
{
    frame_out.width = 1;
    frame_out.height = 1;
    frame_out.pixel_format = PixelFormat::BGR24;
    frame_out.timestamp = timestamp_now();
    frame_out.data = {0, 0, 0};
    frame_out.jpeg_fallback.assign(MINIMAL_JPEG, MINIMAL_JPEG + MINIMAL_JPEG_SIZE);
    return true;
}

// --- CameraStreamer ---
CameraStreamer::CameraStreamer(std::shared_ptr<IMqttClient> mqtt, const std::string& twin_uuid,
                               std::shared_ptr<IFrameSource> source, int fps, std::string sensor_name,
                               bool enable_webrtc, bool enable_mqtt_video_fallback, std::string webrtc_stun_url,
                               std::vector<std::string> webrtc_turn_servers)
    : mqtt_(std::move(mqtt)), twin_uuid_(twin_uuid), source_(std::move(source)), fps_(fps > 0 ? fps : 30),
      sensor_name_(std::move(sensor_name)), enable_webrtc_(enable_webrtc),
      enable_mqtt_video_fallback_(enable_mqtt_video_fallback), webrtc_stun_url_(std::move(webrtc_stun_url)),
      webrtc_turn_servers_(std::move(webrtc_turn_servers))
{
}

CameraStreamer::~CameraStreamer() { stop(); }

void CameraStreamer::set_log_callback(std::function<void(const std::string&)> fn) { log_fn_ = std::move(fn); }

void CameraStreamer::start()
{
    if (running_.exchange(true))
        return;
    if (!mqtt_ || !source_)
    {
        running_ = false;
        return;
    }

    if (enable_webrtc_ && !webrtc_adapter_)
    {
        try
        {
            webrtc_mqtt_subscription_ =
                mqtt_->subscribe_webrtc_messages_scoped(twin_uuid_,
                                                        [this](const std::string& json_payload)
                                                        {
                                                            if (!webrtc_adapter_)
                                                            {
                                                                return;
                                                            }
                                                            try
                                                            {
                                                                Json msg = Json::parse(json_payload);
                                                                if (msg.contains("type") && msg["type"].is_string())
                                                                {
                                                                    const std::string type =
                                                                        msg["type"].get<std::string>();
                                                                    if (type == "answer")
                                                                    {
                                                                        webrtc_adapter_->handle_answer(msg);
                                                                        return;
                                                                    }
                                                                    if (type == "candidate")
                                                                    {
                                                                        webrtc_adapter_->handle_candidate(msg);
                                                                        return;
                                                                    }
                                                                }
                                                            }
                                                            catch (...)
                                                            {
                                                                // ignore malformed payloads
                                                            }
                                                        });

            webrtc_adapter_ = create_webrtc_adapter();
            if (!webrtc_adapter_)
            {
                throw std::runtime_error("WebRTC adapter unavailable (rtc.hpp not available)");
            }
            WebRTCAdapter::Config cfg;
            cfg.sensor = sensor_name_;
            cfg.stun_url = webrtc_stun_url_;
            cfg.turn_servers = webrtc_turn_servers_;
            cfg.recording = recording_;
            cfg.log_fn = log_fn_;
            webrtc_adapter_->start(cfg,
                                   [this](const Json& payload)
                                   {
                                       if (!mqtt_)
                                       {
                                           return;
                                       }
                                       mqtt_->publish_webrtc_message(twin_uuid_, payload.dump());
                                   });
        }
        catch (...)
        {
            // If libdatachannel isn't available at runtime or SDP negotiation fails,
            // keep MQTT /video publishing working.
            webrtc_mqtt_subscription_.reset();
            enable_webrtc_ = false;
            webrtc_adapter_.reset();
        }
    }

    // New session: reset frame counter + drop encoder state so we re-init
    // on the first real frame (resolution changes, encoder renegotiation, etc).
    frame_counter_ = 0;
    h264_encoder_.reset();
    edge_health_stream_started_at_seconds_ = timestamp_now();
    edge_health_last_publish_ts_seconds_ = 0.0;
    edge_health_last_frame_ts_seconds_ = 0.0;
    edge_health_frames_sent_ = 0;

    thread_ = std::thread(&CameraStreamer::stream_loop, this);
}

void CameraStreamer::stop()
{
    running_ = false;
    if (thread_.joinable())
    {
        thread_.join();
        thread_ = std::thread();
    }

    if (webrtc_adapter_)
    {
        try
        {
            webrtc_adapter_->stop();
        }
        catch (...)
        {
        }
        webrtc_adapter_.reset();
    }
    webrtc_mqtt_subscription_.reset();

    h264_encoder_.reset();
    frame_counter_ = 0;
    edge_health_stream_started_at_seconds_ = 0.0;
    edge_health_last_publish_ts_seconds_ = 0.0;
    edge_health_last_frame_ts_seconds_ = 0.0;
    edge_health_frames_sent_ = 0;
}

void CameraStreamer::stream_loop()
{
    const auto frame_interval = std::chrono::duration<double>(1.0 / fps_);
    const bool require_mqtt = !enable_webrtc_;
    while (running_ && source_ && (!require_mqtt || (mqtt_ && mqtt_->is_connected())))
    {
        auto frame_start = std::chrono::steady_clock::now();
        VideoFrame frame;
        if (source_->next_frame(frame) && !frame.data.empty())
        {
            bool sent_via_webrtc = false;
            bool sent_to_cloud = false;
            if (enable_webrtc_ && webrtc_adapter_)
            {
                // Use actual wall-clock time for RTP timestamps so the browser's
                // jitter buffer sees timestamps that match real frame arrival timing.
                // A frame-counter * (1/fps) approach drifts when the source delivers
                // at a different rate than the configured fps (e.g. 25fps MJPEG vs
                // 30fps config), causing the jitter buffer to grow unboundedly.
                const std::uint64_t webrtc_ts_us = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(frame_start.time_since_epoch()).count());

                if (!h264_encoder_)
                {
                    try
                    {
                        h264_encoder_ = std::make_unique<JpegToH264EncoderImpl>(fps_);
                    }
                    catch (...)
                    {
                        h264_encoder_.reset();
                    }
                }

                if (h264_encoder_)
                {
                    std::vector<std::uint8_t> h264_bytes;
                    const bool ok = h264_encoder_->encode_to_annexb_h264(frame, h264_bytes);
                    if (ok && !h264_bytes.empty())
                    {
                        sent_via_webrtc = webrtc_adapter_->send_frame(h264_bytes, webrtc_ts_us);
                        if (sent_via_webrtc)
                        {
                            frame_counter_++;
                            sent_to_cloud = true;
                        }
                    }
                }
            }

            if (!sent_via_webrtc && enable_mqtt_video_fallback_ && mqtt_ && mqtt_->is_connected() &&
                !frame.jpeg_fallback.empty())
            {
                const double now_ts = timestamp_now();
                if ((now_ts - mqtt_fallback_last_send_ts_) >= mqtt_fallback_min_interval_s_)
                {
                    std::string b64 = base64_encode(frame.jpeg_fallback.data(), frame.jpeg_fallback.size());
                    web::json::value payload = web::json::value::object();
                    payload[from_std("type")] = web::json::value::string(from_std("jpeg"));
                    payload[from_std("data")] = web::json::value::string(from_std(b64));
                    payload[from_std("timestamp")] = web::json::value::number(now_ts);
                    std::string topic = mqtt_->get_topic_prefix() + "cyberwave/twin/" + twin_uuid_ + "/video";
                    mqtt_->publish(topic, json_serialize(payload));
                    mqtt_fallback_last_send_ts_ = now_ts;
                    sent_to_cloud = true;
                }
            }

            if (sent_to_cloud)
            {
                edge_health_frames_sent_ += 1;
                edge_health_last_frame_ts_seconds_ = timestamp_now();
            }
        }

        const double now = timestamp_now();
        if (mqtt_ && edge_health_stream_started_at_seconds_ > 0.0 &&
            (edge_health_last_publish_ts_seconds_ <= 0.0 ||
             (now - edge_health_last_publish_ts_seconds_) >= edge_health_publish_interval_seconds))
        {
            publish_camera_stream_edge_health(*mqtt_, twin_uuid_, now,
                                              {edge_health_stream_started_at_seconds_,
                                               edge_health_last_frame_ts_seconds_, edge_health_frames_sent_,
                                               sensor_name_});
            edge_health_last_publish_ts_seconds_ = now;
        }

        auto elapsed = std::chrono::steady_clock::now() - frame_start;
        auto sleep_duration = frame_interval - elapsed;
        if (sleep_duration.count() > 0)
            std::this_thread::sleep_for(
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(sleep_duration));
    }
    running_ = false;
}

// --- EncodedH264CameraStreamer ---
EncodedH264CameraStreamer::EncodedH264CameraStreamer(std::shared_ptr<IMqttClient> mqtt, const std::string& twin_uuid,
                                                     std::string sensor_name, std::string webrtc_stun_url,
                                                     std::vector<std::string> webrtc_turn_servers)
    : mqtt_(std::move(mqtt)), twin_uuid_(twin_uuid), sensor_name_(std::move(sensor_name)),
      webrtc_stun_url_(std::move(webrtc_stun_url)), webrtc_turn_servers_(std::move(webrtc_turn_servers))
{
}

EncodedH264CameraStreamer::~EncodedH264CameraStreamer() { stop(); }

void EncodedH264CameraStreamer::set_log_callback(std::function<void(const std::string&)> fn)
{
    log_fn_ = std::move(fn);
}

void EncodedH264CameraStreamer::start()
{
    if (running_.exchange(true))
        return;
    if (!mqtt_)
    {
        running_ = false;
        return;
    }

    try
    {
        webrtc_mqtt_subscription_ =
            mqtt_->subscribe_webrtc_messages_scoped(twin_uuid_,
                                                    [this](const std::string& json_payload)
                                                    {
                                                        if (!webrtc_adapter_)
                                                        {
                                                            return;
                                                        }
                                                        try
                                                        {
                                                            Json msg = Json::parse(json_payload);
                                                            if (msg.contains("type") && msg["type"].is_string())
                                                            {
                                                                const std::string type = msg["type"].get<std::string>();
                                                                if (type == "answer")
                                                                {
                                                                    webrtc_adapter_->handle_answer(msg);
                                                                    return;
                                                                }
                                                                if (type == "candidate")
                                                                {
                                                                    webrtc_adapter_->handle_candidate(msg);
                                                                    return;
                                                                }
                                                            }
                                                        }
                                                        catch (...)
                                                        {
                                                        }
                                                    });

        webrtc_adapter_ = create_webrtc_adapter();
        if (!webrtc_adapter_)
        {
            throw std::runtime_error("WebRTC adapter unavailable (rtc.hpp not available)");
        }

        WebRTCAdapter::Config cfg;
        cfg.sensor = sensor_name_;
        cfg.stun_url = webrtc_stun_url_;
        cfg.turn_servers = webrtc_turn_servers_;
        cfg.recording = recording_;
        cfg.log_fn = log_fn_;
        webrtc_adapter_->start(cfg,
                               [this](const Json& payload)
                               {
                                   if (mqtt_)
                                   {
                                       mqtt_->publish_webrtc_message(twin_uuid_, payload.dump());
                                   }
                               });

        edge_health_stream_started_at_seconds_ = timestamp_now();
        edge_health_last_publish_ts_seconds_ = 0.0;
        edge_health_last_frame_ts_seconds_ = 0.0;
        edge_health_frames_sent_ = 0;
    }
    catch (...)
    {
        webrtc_mqtt_subscription_.reset();
        webrtc_adapter_.reset();
        running_ = false;
    }
}

void EncodedH264CameraStreamer::stop()
{
    running_ = false;
    if (webrtc_adapter_)
    {
        try
        {
            webrtc_adapter_->stop();
        }
        catch (...)
        {
        }
        webrtc_adapter_.reset();
    }
    webrtc_mqtt_subscription_.reset();
    edge_health_stream_started_at_seconds_ = 0.0;
    edge_health_last_publish_ts_seconds_ = 0.0;
    edge_health_last_frame_ts_seconds_ = 0.0;
    edge_health_frames_sent_ = 0;
}

bool EncodedH264CameraStreamer::send_frame(const std::vector<std::uint8_t>& annexb_h264, std::uint64_t timestamp_us)
{
    if (!running_ || !webrtc_adapter_ || annexb_h264.empty())
    {
        return false;
    }

    // Passthrough carries the robot's RTP clock; the browser jitter buffer expects
    // timestamps aligned with wall-clock frame arrival (same as CameraStreamer).
    (void)timestamp_us;
    const std::uint64_t webrtc_ts_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
    const bool sent = webrtc_adapter_->send_frame(annexb_h264, webrtc_ts_us);

    const double now = timestamp_now();
    if (sent)
    {
        edge_health_frames_sent_ += 1;
        edge_health_last_frame_ts_seconds_ = now;
    }

    if (mqtt_ && edge_health_stream_started_at_seconds_ > 0.0 &&
        (edge_health_last_publish_ts_seconds_ <= 0.0 ||
         (now - edge_health_last_publish_ts_seconds_) >= edge_health_publish_interval_seconds))
    {
        publish_camera_stream_edge_health(*mqtt_, twin_uuid_, now,
                                          {edge_health_stream_started_at_seconds_, edge_health_last_frame_ts_seconds_,
                                           edge_health_frames_sent_, sensor_name_});
        edge_health_last_publish_ts_seconds_ = now;
    }

    return sent;
}

// --- VirtualDepthSource ---
bool VirtualDepthSource::next_depth_frame(DepthFrame& frame_out)
{
    frame_out.width = 4;
    frame_out.height = 4;
    frame_out.timestamp = timestamp_now();
    frame_out.data.assign(16, 1000); // 1000 mm = 1 m flat plane
    return true;
}

// --- DepthStreamer ---
DepthStreamer::DepthStreamer(std::shared_ptr<IMqttClient> mqtt, const std::string& twin_uuid,
                             std::shared_ptr<IDepthSource> source, int fps, bool publish_depth, bool publish_pointcloud)
    : mqtt_(std::move(mqtt)), twin_uuid_(twin_uuid), source_(std::move(source)), fps_(fps > 0 ? fps : 10),
      publish_depth_(publish_depth), publish_pointcloud_(publish_pointcloud)
{
}

DepthStreamer::~DepthStreamer() { stop(); }

void DepthStreamer::start()
{
    if (running_.exchange(true))
        return;
    if (!mqtt_ || !source_)
    {
        running_ = false;
        return;
    }
    if (publish_pointcloud_)
    {
        pc_publish_thread_ = std::thread(&DepthStreamer::pointcloud_publish_loop, this);
    }
    thread_ = std::thread(&DepthStreamer::stream_loop, this);
}

void DepthStreamer::stop()
{
    running_ = false;
    pc_cv_.notify_all();
    if (pc_publish_thread_.joinable())
    {
        pc_publish_thread_.join();
        pc_publish_thread_ = std::thread();
    }
    if (thread_.joinable())
    {
        thread_.join();
        thread_ = std::thread();
    }
}

void DepthStreamer::pointcloud_publish_loop()
{
    while (running_ && mqtt_ && mqtt_->is_connected())
    {
        std::string topic;
        std::string payload;
        {
            std::unique_lock<std::mutex> lock(pc_mutex_);
            pc_cv_.wait(lock, [this] { return pc_has_pending_ || !running_; });
            if (!running_ && !pc_has_pending_)
                break;
            topic = std::move(pc_pending_topic_);
            payload = std::move(pc_pending_payload_);
            pc_has_pending_ = false;
        }
        try
        {
            mqtt_->publish(topic, payload);
        }
        catch (...)
        {
        }
    }
}

void DepthStreamer::stream_loop()
{
    const auto frame_interval = std::chrono::duration<double>(1.0 / fps_);
    while (running_ && mqtt_ && mqtt_->is_connected())
    {
        auto frame_start = std::chrono::steady_clock::now();

        // Depth frame → /depth
        DepthFrame frame;
        if (publish_depth_ && source_ && source_->next_depth_frame(frame) && !frame.data.empty())
        {
            std::string b64 = base64_encode(reinterpret_cast<const unsigned char*>(frame.data.data()),
                                            frame.data.size() * sizeof(uint16_t));
            web::json::value data = web::json::value::object();
            data[from_std("width")] = frame.width;
            data[from_std("height")] = frame.height;
            data[from_std("dtype")] = web::json::value::string(from_std("uint16"));
            data[from_std("depth_binary")] = web::json::value::string(from_std(b64));
            web::json::value payload = web::json::value::object();
            payload[from_std("type")] = web::json::value::string(from_std("depth_data"));
            payload[from_std("data")] = data;
            payload[from_std("timestamp")] = frame.timestamp;
            std::string depth_topic = mqtt_->get_topic_prefix() + "cyberwave/twin/" + twin_uuid_ + "/depth";
            mqtt_->publish(depth_topic, json_serialize(payload));
        }

        // Pointcloud → hand off to the dedicated publish thread (non-blocking).
        // The slot overwrites any unsent payload, so the publisher always sends
        // the freshest frame and stale data is dropped — not queued.
        std::vector<PointXYZRGB> cloud;
        if (publish_pointcloud_ && source_ && source_->next_point_cloud(cloud) && !cloud.empty())
        {
            std::string bytes_b64 =
                base64_encode(reinterpret_cast<const unsigned char*>(cloud.data()), cloud.size() * sizeof(PointXYZRGB));
            web::json::value pc_payload = web::json::value::object();
            pc_payload[from_std("type")] = web::json::value::string(from_std("pointcloud"));
            pc_payload[from_std("data")] = web::json::value::string(from_std(bytes_b64));
            pc_payload[from_std("point_count")] = web::json::value::number(static_cast<int>(cloud.size()));
            pc_payload[from_std("point_stride")] = web::json::value::number(6);
            pc_payload[from_std("timestamp")] = timestamp_now();
            std::string pc_topic = mqtt_->get_topic_prefix() + "cyberwave/twin/" + twin_uuid_ + "/pointcloud";
            {
                std::lock_guard<std::mutex> lock(pc_mutex_);
                pc_pending_topic_ = std::move(pc_topic);
                pc_pending_payload_ = json_serialize(pc_payload);
                pc_has_pending_ = true;
            }
            pc_cv_.notify_one();
        }

        auto elapsed = std::chrono::steady_clock::now() - frame_start;
        auto sleep_dur = frame_interval - elapsed;
        if (sleep_dur.count() > 0)
            std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::steady_clock::duration>(sleep_dur));
    }
    running_ = false;
    pc_cv_.notify_all();
}

} // namespace cyberwave
