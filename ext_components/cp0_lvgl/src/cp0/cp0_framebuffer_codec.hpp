#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cp0::framebuffer {

struct Channel
{
    uint8_t offset = 0;
    uint8_t length = 0;
};

struct Layout
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t x_offset = 0;
    uint32_t y_offset = 0;
    uint32_t bits_per_pixel = 0;
    size_t line_length = 0;
    Channel red;
    Channel green;
    Channel blue;
};

bool encode_ppm(const uint8_t *data,
                size_t data_size,
                const Layout &layout,
                std::vector<uint8_t> &out,
                std::string &error);

} // namespace cp0::framebuffer
