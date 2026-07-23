#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#include <list>
#include <string>

extern "C" {
#endif

int cp0_display_dimension_or_default(const char *text, int default_value);

#ifdef __cplusplus
}

namespace cp0::screenshot {

struct Request
{
    std::string directory;
};

struct Timestamp
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
};

struct FramebufferLayout
{
    std::size_t mapped_size = 0;
    std::uint32_t bmp_row_size = 0;
    std::uint32_t bmp_image_size = 0;
    std::uint32_t bmp_file_size = 0;
};

bool parse_request(const std::list<std::string> &args, Request &request);
const char *invalid_request_message();
bool valid_dimensions(int width, int height);
bool make_output_path(const std::string &directory, const Timestamp &timestamp,
    bool sdl_backend, std::string &path);
bool framebuffer_layout(int width, int height, int bits_per_pixel,
    int line_length, FramebufferLayout &layout);

} // namespace cp0::screenshot
#endif
