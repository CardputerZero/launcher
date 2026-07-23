#include "cp0_framebuffer_codec.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using cp0::framebuffer::Layout;

std::vector<uint8_t> pixels(const std::vector<uint8_t> &ppm)
{
    const std::string header = "P6\n2 1\n255\n";
    assert(ppm.size() >= header.size());
    assert(std::string(ppm.begin(), ppm.begin() + header.size()) == header);
    return {ppm.begin() + header.size(), ppm.end()};
}

void test_rgb565_with_virtual_offset()
{
    const std::vector<uint8_t> framebuffer = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0x00, 0xf8, 0xe0, 0x07, 0, 0,
    };
    const Layout layout {2, 1, 1, 1, 16, 8, {}, {}, {}};
    std::vector<uint8_t> ppm;
    std::string error;
    assert(cp0::framebuffer::encode_ppm(framebuffer.data(), framebuffer.size(), layout,
                                        ppm, error));
    assert((pixels(ppm) == std::vector<uint8_t> {255, 0, 0, 0, 255, 0}));
    assert(error.empty());
}

void test_32_bit_channel_layout()
{
    const std::vector<uint8_t> framebuffer = {
        0x00, 0x00, 0xff, 0x00,
        0xff, 0x00, 0x00, 0x00,
    };
    const Layout layout {2, 1, 0, 0, 32, 8, {16, 8}, {8, 8}, {0, 8}};
    std::vector<uint8_t> ppm;
    std::string error;
    assert(cp0::framebuffer::encode_ppm(framebuffer.data(), framebuffer.size(), layout,
                                        ppm, error));
    assert((pixels(ppm) == std::vector<uint8_t> {255, 0, 0, 0, 0, 255}));
}

void test_rejects_invalid_layout_and_bounds()
{
    const std::vector<uint8_t> framebuffer(8);
    std::vector<uint8_t> ppm {1, 2, 3};
    std::string error;

    assert(!cp0::framebuffer::encode_ppm(framebuffer.data(), framebuffer.size(),
                                         Layout {0, 1, 0, 0, 16, 8, {}, {}, {}},
                                         ppm, error));
    assert(ppm.empty());
    assert(error == "unsupported framebuffer format");

    assert(!cp0::framebuffer::encode_ppm(framebuffer.data(), framebuffer.size(),
                                         Layout {2, 1, 1, 0, 16, 4, {}, {}, {}},
                                         ppm, error));
    assert(error == "framebuffer bounds mismatch");

    assert(!cp0::framebuffer::encode_ppm(framebuffer.data(), framebuffer.size(),
                                         Layout {2, 1, 0, 2, 16, 4, {}, {}, {}},
                                         ppm, error));
    assert(error == "framebuffer bounds mismatch");

    assert(!cp0::framebuffer::encode_ppm(
        framebuffer.data(), framebuffer.size(),
        Layout {1, UINT32_MAX, 0, UINT32_MAX, 16, 2, {}, {}, {}}, ppm, error));
    assert(error == "framebuffer bounds mismatch");
}

} // namespace

int main()
{
    test_rgb565_with_virtual_offset();
    test_32_bit_channel_layout();
    test_rejects_invalid_layout_and_bounds();
    return 0;
}
