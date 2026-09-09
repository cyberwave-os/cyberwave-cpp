#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace cyberwave::detail
{
// Cyberwave SEI v1 wire format, shared with the recording verifier. All fields
// are big-endian; CRC32 covers the first 58 payload bytes (excluding UUID).
inline std::vector<std::uint8_t> capture_sei_payload(std::uint64_t frame_index, std::uint64_t pts_us,
                                                     std::uint64_t wall_ns, std::uint64_t monotonic_ns,
                                                     const std::array<std::uint8_t, 16>& track_id)
{
    std::vector<std::uint8_t> payload{1, 0};
    auto append = [&](std::uint64_t value, int bytes)
    {
        for (int i = bytes - 1; i >= 0; --i)
            payload.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    };
    append(frame_index, 8);
    append(pts_us, 8);
    append(1, 4);
    append(1000000, 4);
    append(wall_ns, 8);
    append(monotonic_ns, 8);
    payload.insert(payload.end(), track_id.begin(), track_id.end());
    std::uint32_t crc = 0xffffffffU;
    for (auto byte : payload)
    {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    append(crc ^ 0xffffffffU, 4);
    return payload;
}

inline std::vector<std::uint8_t> with_capture_sei(const std::vector<std::uint8_t>& annexb, std::uint64_t frame_index,
                                                  std::uint64_t pts_us, std::uint64_t wall_ns,
                                                  std::uint64_t monotonic_ns,
                                                  const std::array<std::uint8_t, 16>& track_id)
{
    // SPS/PPS/AUD remain in their original order. Insert immediately before the
    // first picture NAL so the timestamp belongs to this access unit.
    std::size_t picture = annexb.size();
    for (std::size_t i = 0; i + 3 < annexb.size(); ++i)
    {
        if (annexb[i] || annexb[i + 1])
            continue;
        const std::size_t prefix =
            annexb[i + 2] == 1 ? 3 : (i + 4 < annexb.size() && annexb[i + 2] == 0 && annexb[i + 3] == 1 ? 4 : 0);
        if (prefix && (annexb[i + prefix] & 31) >= 1 && (annexb[i + prefix] & 31) <= 5)
        {
            picture = i;
            break;
        }
    }
    if (picture == annexb.size() || wall_ns == 0)
        return annexb;
    std::vector<std::uint8_t> rbsp{5,    78,   0xf4, 0x7a, 0xc9, 0xe1, 0x3b, 0x2d, 0x4f,
                                   0x8a, 0x9c, 0x1e, 0xd6, 0x8f, 0x4a, 0x2b, 0x7c, 0x3d};
    const auto payload = capture_sei_payload(frame_index, pts_us, wall_ns, monotonic_ns, track_id);
    rbsp.insert(rbsp.end(), payload.begin(), payload.end());
    rbsp.push_back(0x80);
    std::vector<std::uint8_t> result(annexb.begin(), annexb.begin() + picture);
    result.insert(result.end(), {0, 0, 0, 1, 6});
    int zeros = 0;
    for (auto byte : rbsp)
    {
        if (zeros >= 2 && byte <= 3)
        {
            result.push_back(3);
            zeros = 0;
        }
        result.push_back(byte);
        zeros = byte == 0 ? zeros + 1 : 0;
    }
    result.insert(result.end(), annexb.begin() + picture, annexb.end());
    return result;
}
} // namespace cyberwave::detail
