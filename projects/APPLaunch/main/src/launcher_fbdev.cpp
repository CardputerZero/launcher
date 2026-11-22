/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "launcher_fbdev.h"
#include "lvgl/lvgl.h"

#if LV_USE_LINUX_FBDEV && !LV_LINUX_FBDEV_MMAP && defined(__linux__)

#include "lvgl/lvgl_private.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <linux/fb.h>
#include <new>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

constexpr int32_t kPanelWidth = 320;
constexpr int32_t kPanelHeight = 170;
constexpr uint32_t kPanelStride = kPanelWidth * 2;
constexpr unsigned kMaxRedrawRetries = 2;

struct StripeOutput {
    int fd;
    lv_display_flush_cb_t original_flush;
    lv_area_t failed_area{};
    unsigned retries = 0;
    bool wrote_pixels = false;
    bool failed = false;
};

bool compatible_display(lv_display_t *display)
{
    return lv_display_get_horizontal_resolution(display) == kPanelWidth &&
           lv_display_get_vertical_resolution(display) == kPanelHeight &&
           lv_display_get_offset_x(display) == 0 && lv_display_get_offset_y(display) == 0 &&
           lv_display_get_render_mode(display) == LV_DISPLAY_RENDER_MODE_PARTIAL &&
           lv_display_get_rotation(display) == LV_DISPLAY_ROTATION_0 &&
           lv_display_get_color_format(display) == LV_COLOR_FORMAT_RGB565 &&
           display->stride_is_auto;
}

int write_pixels(int fd, const uint8_t *pixels, size_t bytes, off_t offset)
{
    while (bytes != 0) {
        const ssize_t written = pwrite(fd, pixels, bytes, offset);
        if (written < 0 && errno == EINTR) continue;
        if (written < 0) return errno;
        if (written == 0) return EIO;
        pixels += written;
        bytes -= written;
        offset += written;
    }
    return 0;
}

void flush_stripe(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    auto *output = static_cast<StripeOutput *>(lv_display_get_user_data(display));
    if (!compatible_display(display) || area->x1 < 0 || area->x2 >= kPanelWidth ||
        area->y1 < 0 || area->y2 >= kPanelHeight) {
        output->original_flush(display, area, pixels);
        return;
    }

    const uint32_t width = lv_area_get_width(area);
    const uint32_t row_bytes = width * 2;
    const uint32_t height = lv_area_get_height(area);
    off_t offset = static_cast<off_t>(area->y1) * kPanelStride + area->x1 * 2;

    int error = 0;
    if (area->x1 == 0 && row_bytes == kPanelStride) {
        error = write_pixels(output->fd, pixels, row_bytes * height, offset);
    } else {
        const uint32_t source_stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565);
        for (uint32_t row = 0; row < height && !error; ++row) {
            error = write_pixels(output->fd, pixels, row_bytes, offset);
            pixels += source_stride;
            offset += kPanelStride;
        }
    }

    output->wrote_pixels = true;
    if (error) {
        if (output->failed) {
            lv_area_join(&output->failed_area, &output->failed_area, area);
        } else {
            output->failed_area = *area;
            output->failed = true;
            if (output->retries == 0)
                std::fprintf(stderr, "APPLaunch: framebuffer write failed (errno %d), scheduling redraw\n",
                             error);
        }
    }
    lv_display_flush_ready(display);
}

void retry_failed_redraw(lv_display_t *display, StripeOutput &output)
{
    if (!output.wrote_pixels) return;
    output.wrote_pixels = false;
    if (!output.failed) {
        output.retries = 0;
        return;
    }
    output.failed = false;

    // REFR_READY comes after LVGL clears the invalidated areas for this frame.
    // Only a successful refresh with actual writes re-arms the retry budget.
    if (output.retries < kMaxRedrawRetries) {
        if (lv_inv_area(display, &output.failed_area) == LV_RESULT_OK)
            ++output.retries;
    } else if (output.retries == kMaxRedrawRetries) {
        ++output.retries;
        std::fprintf(stderr, "APPLaunch: framebuffer redraw retry limit reached\n");
    }
}

void on_display_event(lv_event_t *event)
{
    auto *display = static_cast<lv_display_t *>(lv_event_get_target(event));
    auto *output = static_cast<StripeOutput *>(lv_event_get_user_data(event));
    switch (lv_event_get_code(event)) {
    case LV_EVENT_INVALIDATE_AREA: {
        if (!compatible_display(display)) break;
        // Render the full affected rows so we can submit each stripe in one write.
        auto *area = static_cast<lv_area_t *>(lv_event_get_param(event));
        area->x1 = 0;
        area->x2 = lv_display_get_horizontal_resolution(display) - 1;
        break;
    }
    case LV_EVENT_REFR_READY:
        retry_failed_redraw(display, *output);
        break;
    case LV_EVENT_DELETE: {
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
    if (!display || !device || lv_display_get_user_data(display) ||
        !display->flush_cb || !compatible_display(display)) return;

    const int fd = open(device, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return;
    fb_fix_screeninfo fixed{};
    fb_var_screeninfo var{};
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fixed) < 0 ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 ||
        var.xres != kPanelWidth || var.yres != kPanelHeight || var.xoffset != 0 || var.yoffset != 0 ||
        var.bits_per_pixel != 16 || var.red.offset != 11 || var.green.offset != 5 ||
        var.blue.offset != 0 || fixed.line_length != kPanelStride ||
        fixed.smem_len < kPanelStride * kPanelHeight ||
        lv_draw_buf_width_to_stride(var.xres, LV_COLOR_FORMAT_RGB565) != fixed.line_length) {
        close(fd);
        return;
    }

    auto *output = new (std::nothrow) StripeOutput{fd, display->flush_cb};
    if (!output) {
        close(fd);
        return;
    }
    const uint32_t event_count = lv_display_get_event_count(display);
    lv_display_add_event_cb(display, on_display_event, LV_EVENT_ALL, output);
    if (lv_display_get_event_count(display) == event_count) {
        close(fd);
        delete output;
        return;
    }
    lv_display_set_user_data(display, output);
    lv_display_set_flush_cb(display, flush_stripe);
}

bool launcher_fbdev_stripes_active()
{
    auto *display = lv_display_get_default();
    return display && display->flush_cb == flush_stripe && compatible_display(display);
}

#else

void launcher_enable_fbdev_stripes() {}
bool launcher_fbdev_stripes_active() { return false; }

#endif
