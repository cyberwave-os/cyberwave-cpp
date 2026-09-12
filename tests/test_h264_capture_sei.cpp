#include "../src/cyberwave/h264_capture_sei.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

static void check(bool value)
{
    if (!value)
    {
        std::fprintf(stderr, "Capture SEI regression failed\n");
        std::abort();
    }
}

int main()
{
    const std::array<std::uint8_t, 16> track{};
    const auto payload = cyberwave::detail::capture_sei_payload(2, 100000, 1788890000000000000ULL, 100000000, track);
    check(payload.size() == 62);
    check(payload[0] == 1 && payload[1] == 0 && payload[9] == 2);
    // Cross-language CRC32 golden, computed by the backend's zlib parser.
    check(payload[58] == 171 && payload[59] == 69 && payload[60] == 66 && payload[61] == 170);
    const std::vector<std::uint8_t> original{0, 0, 0, 1, 9, 0xf0, 0, 0, 1, 7, 9, 0, 0, 0, 1, 0x65, 0xaa};
    const auto wrapped =
        cyberwave::detail::with_capture_sei(original, 2, 100000, 1788890000000000000ULL, 100000000, track);
    check(std::equal(original.begin(), original.begin() + 11, wrapped.begin()));
    check(std::equal(original.begin() + 11, original.end(), wrapped.end() - 6));
    check(wrapped[15] == 6);
    // Zero-filled fields must be escaped rather than introducing Annex-B NAL delimiters.
    for (std::size_t i = 16; i + 2 < wrapped.size() - 6; ++i)
        check(!(wrapped[i] == 0 && wrapped[i + 1] == 0 && wrapped[i + 2] <= 2));
    check(cyberwave::detail::with_capture_sei(original, 0, 0, 0, 0, track) == original);
    const std::vector<std::uint8_t> config_only{0, 0, 1, 7, 9};
    check(cyberwave::detail::with_capture_sei(config_only, 0, 0, 1, 0, track) == config_only);
    std::puts("Capture SEI regression passed");
}
