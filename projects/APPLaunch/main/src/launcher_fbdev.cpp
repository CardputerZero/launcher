/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "launcher_fbdev.h"
#include "lvgl/lvgl.h"

#if LV_USE_LINUX_FBDEV && !LV_LINUX_FBDEV_MMAP && defined(__linux__)

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <linux/fb.h>
#include <new>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

struct StripeOutput {
    int fd;
    uint32_t stride;
};

bool write_pixels(int fd, const uint8_t *pixels, size_t bytes, off_t offset)
{
    while (bytes != 0) {
        const ssize_t written = pwrite(fd, pixels, bytes, offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        pixels += written;
        bytes -= written;
        offset += written;
    }
    return true;
}

void flush_stripe(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    const auto *output = static_cast<const StripeOutput *>(lv_display_get_user_data(display));
    const uint32_t width = lv_area_get_width(area);
    const uint32_t row_bytes = width * 2;
    const uint32_t height = lv_area_get_height(area);
    off_t offset = static_cast<off_t>(area->y1) * output->stride + area->x1 * 2;

    bool ok;
    if (area->x1 == 0 && row_bytes == output->stride) {
        ok = write_pixels(output->fd, pixels, row_bytes * height, offset);
    } else {
        const uint32_t source_stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565);
        ok = true;
        for (uint32_t row = 0; row < height && ok; ++row) {
            ok = write_pixels(output->fd, pixels, row_bytes, offset);
            pixels += source_stride;
            offset += output->stride;
        }
    }
    if (!ok) LV_LOG_ERROR("APPLaunch: framebuffer write failed");
    lv_display_flush_ready(display);
}

void on_display_event(lv_event_t *event)
{
    auto *display = static_cast<lv_display_t *>(lv_event_get_target(event));
    switch (lv_event_get_code(event)) {
    case LV_EVENT_INVALIDATE_AREA: {
        // Render the full affected rows so we can submit each stripe in one write.
        auto *area = static_cast<lv_area_t *>(lv_event_get_param(event));
        area->x1 = 0;
        area->x2 = lv_display_get_horizontal_resolution(display) - 1;
        break;
    }
    case LV_EVENT_DELETE: {
        auto *output = static_cast<StripeOutput *>(lv_display_get_user_data(display));
        close(output->fd);
        delete output;
        lv_display_set_user_data(display, nullptr);
        break;
    }
    default:
        break;
    }
}

} // namespace

void launcher_enable_fbdev_stripes()
{
    lv_display_t *display = lv_display_get_default();
    const char *device = std::getenv("LV_LINUX_FBDEV_DEVICE");
    if (!display || !device || lv_display_get_user_data(display)) return;

    const int fd = open(device, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return;
    fb_fix_screeninfo fixed{};
    fb_var_screeninfo var{};
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fixed) < 0 ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 ||
        var.xres != 320 || var.yres != 170 || var.xoffset != 0 || var.yoffset != 0 ||
        var.bits_per_pixel != 16 || var.red.offset != 11 || var.green.offset != 5 ||
        var.blue.offset != 0 || fixed.line_length != 640 || fixed.smem_len < 640 * 170 ||
        lv_display_get_rotation(display) != LV_DISPLAY_ROTATION_0 ||
        lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB565 ||
        lv_draw_buf_width_to_stride(var.xres, LV_COLOR_FORMAT_RGB565) != fixed.line_length) {
        close(fd);
        return;
    }

    auto *output = new (std::nothrow) StripeOutput{fd, fixed.line_length};
    if (!output) {
        close(fd);
        return;
    }
    lv_display_set_user_data(display, output);
    lv_display_add_event_cb(display, on_display_event, LV_EVENT_ALL, nullptr);
    lv_display_set_flush_cb(display, flush_stripe);
}

#else

void launcher_enable_fbdev_stripes() {}

#endif
