#include "cp0_display_screenshot_contract.hpp"

#include <charconv>
#include <cstdio>
#include <limits>
#include <string_view>

namespace {

constexpr int kMaximumDimension = 16384;
constexpr std::size_t kMaximumPathLength = 511;

bool checked_multiply(std::size_t left, std::size_t right, std::size_t &result)
{
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left)
        return false;
    result = left * right;
    return true;
}

} // namespace

extern "C" int cp0_display_dimension_or_default(const char *text, int default_value)
{
    if (!text || !text[0])
        return default_value;
    const std::string_view value(text);
    int parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        parsed <= 0 || parsed > kMaximumDimension)
        return default_value;
    return parsed;
}

namespace cp0::screenshot {

bool parse_request(const std::list<std::string> &args, Request &request)
{
    request = {};
    if (args.size() != 2 || args.front() != "Save")
        return false;
    request.directory = args.back();
    return !request.directory.empty() && request.directory.size() <= kMaximumPathLength;
}

const char *invalid_request_message()
{
    return "invalid screenshot api request\n";
}

bool valid_dimensions(int width, int height)
{
    return width > 0 && width <= kMaximumDimension && height > 0 && height <= kMaximumDimension;
}

bool make_output_path(const std::string &directory, const Timestamp &timestamp,
    bool sdl_backend, std::string &path)
{
    path.clear();
    if (directory.empty() || timestamp.year < 1900 || timestamp.year > 9999 ||
        timestamp.month < 1 || timestamp.month > 12 || timestamp.day < 1 || timestamp.day > 31 ||
        timestamp.hour < 0 || timestamp.hour > 23 || timestamp.minute < 0 || timestamp.minute > 59 ||
        timestamp.second < 0 || timestamp.second > 60)
        return false;
    char suffix[64];
    const int length = std::snprintf(suffix, sizeof(suffix),
        "/scr_%04d%02d%02d_%02d%02d%02d%s.bmp", timestamp.year, timestamp.month,
        timestamp.day, timestamp.hour, timestamp.minute, timestamp.second,
        sdl_backend ? "_sdl" : "");
    if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(suffix) ||
        directory.size() + static_cast<std::size_t>(length) > kMaximumPathLength)
        return false;
    path = directory + suffix;
    return true;
}

bool framebuffer_layout(int width, int height, int bits_per_pixel,
    int line_length, FramebufferLayout &layout)
{
    layout = {};
    if (!valid_dimensions(width, height) || (bits_per_pixel != 16 && bits_per_pixel != 32))
        return false;
    const std::size_t minimum_line = static_cast<std::size_t>(width) *
        static_cast<std::size_t>(bits_per_pixel / 8);
    if (line_length < 0 || static_cast<std::size_t>(line_length) < minimum_line)
        return false;
    if (!checked_multiply(static_cast<std::size_t>(line_length),
            static_cast<std::size_t>(height), layout.mapped_size))
        return false;

    const std::size_t raw_row = static_cast<std::size_t>(width) * 3U;
    const std::size_t bmp_row = (raw_row + 3U) & ~std::size_t{3U};
    std::size_t image_size = 0;
    if (!checked_multiply(bmp_row, static_cast<std::size_t>(height), image_size) ||
        image_size > std::numeric_limits<std::uint32_t>::max() - 54U ||
        bmp_row > std::numeric_limits<std::uint32_t>::max())
        return false;
    layout.bmp_row_size = static_cast<std::uint32_t>(bmp_row);
    layout.bmp_image_size = static_cast<std::uint32_t>(image_size);
    layout.bmp_file_size = static_cast<std::uint32_t>(image_size + 54U);
    return true;
}

} // namespace cp0::screenshot
