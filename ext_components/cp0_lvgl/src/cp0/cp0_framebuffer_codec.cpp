#include "cp0_framebuffer_codec.hpp"

#include <cstring>
#include <limits>

namespace cp0::framebuffer {
namespace {

bool checked_add(size_t lhs, size_t rhs, size_t &result)
{
    if (lhs > std::numeric_limits<size_t>::max() - rhs) return false;
    result = lhs + rhs;
    return true;
}

bool checked_multiply(size_t lhs, size_t rhs, size_t &result)
{
    if (lhs != 0 && rhs > std::numeric_limits<size_t>::max() / lhs) return false;
    result = lhs * rhs;
    return true;
}

uint8_t expand_channel(uint32_t pixel, Channel channel)
{
    if (channel.length == 0 || channel.length > 8 || channel.offset >= 32 ||
        channel.length > 32 - channel.offset)
        return 0;
    const uint32_t maximum = (uint32_t {1} << channel.length) - 1;
    return static_cast<uint8_t>(((pixel >> channel.offset) & maximum) * 255 / maximum);
}

} // namespace

bool encode_ppm(const uint8_t *data,
                size_t data_size,
                const Layout &layout,
                std::vector<uint8_t> &out,
                std::string &error)
{
    out.clear();
    error.clear();
    if (!data || layout.width == 0 || layout.height == 0 ||
        (layout.bits_per_pixel != 16 && layout.bits_per_pixel != 32)) {
        error = "unsupported framebuffer format";
        return false;
    }

    const size_t pixel_bytes = layout.bits_per_pixel / 8;
    size_t row_pixels_bytes = 0;
    size_t x_offset_bytes = 0;
    if (!checked_multiply(layout.width, pixel_bytes, row_pixels_bytes) ||
        !checked_multiply(layout.x_offset, pixel_bytes, x_offset_bytes) ||
        x_offset_bytes > layout.line_length ||
        row_pixels_bytes > layout.line_length - x_offset_bytes) {
        error = "framebuffer bounds mismatch";
        return false;
    }

    size_t last_source_y = 0;
    size_t last_row_offset = 0;
    if (!checked_add(layout.y_offset, layout.height - 1, last_source_y) ||
        !checked_multiply(last_source_y, layout.line_length, last_row_offset) ||
        !checked_add(last_row_offset, x_offset_bytes, last_row_offset) ||
        last_row_offset > data_size || row_pixels_bytes > data_size - last_row_offset) {
        error = "framebuffer bounds mismatch";
        return false;
    }

    size_t pixel_count = 0;
    size_t output_bytes = 0;
    if (!checked_multiply(layout.width, layout.height, pixel_count) ||
        !checked_multiply(pixel_count, size_t {3}, output_bytes)) {
        error = "framebuffer dimensions overflow";
        return false;
    }
    const std::string header = "P6\n" + std::to_string(layout.width) + " " +
                               std::to_string(layout.height) + "\n255\n";
    size_t total_size = 0;
    if (!checked_add(header.size(), output_bytes, total_size)) {
        error = "framebuffer dimensions overflow";
        return false;
    }

    out.assign(header.begin(), header.end());
    out.resize(total_size);
    uint8_t *destination = out.data() + header.size();
    for (uint32_t y = 0; y < layout.height; ++y) {
        size_t source_y = 0;
        size_t row_offset = 0;
        if (!checked_add(y, layout.y_offset, source_y) ||
            !checked_multiply(source_y, layout.line_length, row_offset) ||
            !checked_add(row_offset, x_offset_bytes, row_offset) ||
            row_offset > data_size || row_pixels_bytes > data_size - row_offset) {
            out.clear();
            error = "framebuffer bounds mismatch";
            return false;
        }
        const uint8_t *row = data + row_offset;
        for (uint32_t x = 0; x < layout.width; ++x) {
            uint8_t red = 0;
            uint8_t green = 0;
            uint8_t blue = 0;
            if (layout.bits_per_pixel == 16) {
                uint16_t pixel = 0;
                std::memcpy(&pixel, row + x * 2, sizeof(pixel));
                red = static_cast<uint8_t>(((pixel >> 11) & 0x1f) * 255 / 31);
                green = static_cast<uint8_t>(((pixel >> 5) & 0x3f) * 255 / 63);
                blue = static_cast<uint8_t>((pixel & 0x1f) * 255 / 31);
            } else {
                uint32_t pixel = 0;
                std::memcpy(&pixel, row + x * 4, sizeof(pixel));
                red = expand_channel(pixel, layout.red);
                green = expand_channel(pixel, layout.green);
                blue = expand_channel(pixel, layout.blue);
            }
            *destination++ = red;
            *destination++ = green;
            *destination++ = blue;
        }
    }
    return true;
}

} // namespace cp0::framebuffer
