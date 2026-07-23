#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#if __has_include(<jpeglib.h>) && __has_include(<libcamera/camera.h>)
#define CP0_CAMERA_HAS_LIBCAMERA 1
#include <libcamera/pixel_format.h>

namespace cp0_camera_frame_codec {

bool is_supported_preview_format(const libcamera::PixelFormat &format);
bool convert_to_rgb888(const uint8_t *source, size_t bytes_used, int width, int height,
                       int stride, const libcamera::PixelFormat &format,
                       std::vector<uint8_t> &rgb);
bool convert_to_rgb565(const uint8_t *source, size_t bytes_used, int width, int height,
                       int stride, const libcamera::PixelFormat &format,
                       std::vector<uint16_t> &rgb565);
std::string make_frame_payload(const std::vector<uint16_t> &rgb565, int width, int height);
bool save_jpeg_rgb888(const std::string &path, const uint8_t *rgb, int width, int height,
                      int quality = 90);

} // namespace cp0_camera_frame_codec
#else
#define CP0_CAMERA_HAS_LIBCAMERA 0
#endif
