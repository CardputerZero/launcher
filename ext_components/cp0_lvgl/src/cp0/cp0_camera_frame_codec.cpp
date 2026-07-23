#include "cp0_camera_frame_codec.hpp"

#if CP0_CAMERA_HAS_LIBCAMERA

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>

#include <jpeglib.h>
#include <libcamera/formats.h>

namespace cp0_camera_frame_codec {
namespace {

void rgb565_to_rgb888(uint16_t pixel, uint8_t &red, uint8_t &green, uint8_t &blue)
{
    red = ((pixel >> 11) & 0x1F) << 3;
    green = ((pixel >> 5) & 0x3F) << 2;
    blue = (pixel & 0x1F) << 3;
    red |= red >> 5;
    green |= green >> 6;
    blue |= blue >> 5;
}

uint16_t rgb888_to_rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return static_cast<uint16_t>(((red & 0xF8) << 8) |
                                 ((green & 0xFC) << 3) |
                                 (blue >> 3));
}

} // namespace

bool is_supported_preview_format(const libcamera::PixelFormat &format)
{
    return format == libcamera::formats::RGB888 ||
           format == libcamera::formats::BGR888 ||
           format == libcamera::formats::XRGB8888 ||
           format == libcamera::formats::XBGR8888 ||
           format == libcamera::formats::RGB565;
}

bool convert_to_rgb888(const uint8_t *source, size_t bytes_used, int width, int height,
                       int stride, const libcamera::PixelFormat &format,
                       std::vector<uint8_t> &rgb)
{
    if (!source || width <= 0 || height <= 0)
        return false;
    const bool is_rgb888 = format == libcamera::formats::RGB888;
    const bool is_bgr888 = format == libcamera::formats::BGR888;
    const bool is_xrgb8888 = format == libcamera::formats::XRGB8888;
    const bool is_xbgr8888 = format == libcamera::formats::XBGR8888;
    const bool is_rgb565 = format == libcamera::formats::RGB565;
    const int bytes_per_pixel = is_rgb888 || is_bgr888 ? 3 : is_rgb565 ? 2 : 4;
    const int minimum_stride = width * bytes_per_pixel;
    const int row_stride = stride > 0 ? stride : minimum_stride;
    if (row_stride < minimum_stride)
        return false;

    rgb.assign(width * height * 3, 0);
    for (int y = 0; y < height; ++y) {
        const size_t row_offset = static_cast<size_t>(y) * row_stride;
        if (row_offset + minimum_stride > bytes_used)
            return y > 0;

        const uint8_t *line = source + row_offset;
        const int destination_y = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            const int destination_x = width - 1 - x;
            const int destination = (destination_y * width + destination_x) * 3;
            uint8_t red = 0;
            uint8_t green = 0;
            uint8_t blue = 0;
            if (is_rgb888) {
                const uint8_t *pixel = line + x * 3;
                red = pixel[0]; green = pixel[1]; blue = pixel[2];
            } else if (is_bgr888) {
                const uint8_t *pixel = line + x * 3;
                blue = pixel[0]; green = pixel[1]; red = pixel[2];
            } else if (is_xrgb8888) {
                const uint8_t *pixel = line + x * 4;
                blue = pixel[0]; green = pixel[1]; red = pixel[2];
            } else if (is_xbgr8888) {
                const uint8_t *pixel = line + x * 4;
                red = pixel[0]; green = pixel[1]; blue = pixel[2];
            } else if (is_rgb565) {
                const uint8_t *pixel = line + x * 2;
                rgb565_to_rgb888(static_cast<uint16_t>(pixel[0] | (pixel[1] << 8)),
                                 red, green, blue);
            }
            rgb[destination + 0] = red;
            rgb[destination + 1] = green;
            rgb[destination + 2] = blue;
        }
    }
    return true;
}

bool convert_to_rgb565(const uint8_t *source, size_t bytes_used, int width, int height,
                       int stride, const libcamera::PixelFormat &format,
                       std::vector<uint16_t> &rgb565)
{
    std::vector<uint8_t> rgb;
    if (!convert_to_rgb888(source, bytes_used, width, height, stride, format, rgb))
        return false;
    rgb565.assign(width * height, 0);
    for (int i = 0; i < width * height; ++i)
        rgb565[i] = rgb888_to_rgb565(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
    return true;
}

std::string make_frame_payload(const std::vector<uint16_t> &rgb565, int width, int height)
{
    char header[64];
    const int header_length = std::snprintf(header, sizeof(header),
                                            "FRAME %d %d RGB565\n", width, height);
    std::string payload(header, header_length);
    payload.append(reinterpret_cast<const char *>(rgb565.data()),
                   rgb565.size() * sizeof(uint16_t));
    return payload;
}

bool save_jpeg_rgb888(const std::string &path, const uint8_t *rgb, int width, int height,
                      int quality)
{
    FILE *file = std::fopen(path.c_str(), "wb");
    if (!file)
        return false;

    jpeg_compress_struct compressor;
    jpeg_error_mgr error;
    compressor.err = jpeg_std_error(&error);
    jpeg_create_compress(&compressor);
    jpeg_stdio_dest(&compressor, file);
    compressor.image_width = width;
    compressor.image_height = height;
    compressor.input_components = 3;
    compressor.in_color_space = JCS_RGB;
    jpeg_set_defaults(&compressor);
    jpeg_set_quality(&compressor, quality, TRUE);
    jpeg_start_compress(&compressor, TRUE);
    while (compressor.next_scanline < compressor.image_height) {
        JSAMPROW row[] = {
            const_cast<JSAMPROW>(&rgb[compressor.next_scanline * width * 3])
        };
        jpeg_write_scanlines(&compressor, row, 1);
    }
    jpeg_finish_compress(&compressor);
    jpeg_destroy_compress(&compressor);
    std::fclose(file);
    chmod(path.c_str(), 0666);
    return true;
}

} // namespace cp0_camera_frame_codec

#endif
