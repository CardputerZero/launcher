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

struct BufferIdentity {
    const lv_draw_buf_t *buffer;
    const void *data = buffer ? buffer->data : nullptr;
    uint32_t size = buffer ? buffer->data_size : 0;

    bool matches(const lv_draw_buf_t *current) const
    {
        return current == buffer && (!current || (current->data == data && current->data_size == size));
    }
};

struct StripeOutput {
    int fd;
    lv_display_flush_cb_t original_flush;
    BufferIdentity buffers[3];
    lv_area_t damaged_area{};
    unsigned retries = 0;
    bool has_damage = false;
    bool retry_queued = false;
    bool wrote_pixels = false;
    bool failed_this_refresh = false;
    bool disabled = false;
    bool invalidate_after_disable = false;

    bool buffers_match(const lv_display_t *display) const
    {
        return buffers[0].matches(display->buf_1) && buffers[1].matches(display->buf_2) &&
               buffers[2].matches(display->buf_3);
    }
};

bool compatible_layout(lv_display_t *display)
{
    return lv_display_get_horizontal_resolution(display) == kPanelWidth &&
           lv_display_get_vertical_resolution(display) == kPanelHeight &&
           lv_display_get_render_mode(display) == LV_DISPLAY_RENDER_MODE_PARTIAL &&
           lv_display_get_rotation(display) == LV_DISPLAY_ROTATION_0 &&
           lv_display_get_color_format(display) == LV_COLOR_FORMAT_RGB565 &&
           display->stride_is_auto;
}

bool has_offset(lv_display_t *display)
{
    return lv_display_get_offset_x(display) != 0 || lv_display_get_offset_y(display) != 0;
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
    if (!output->disabled && (!compatible_layout(display) || !output->buffers_match(display))) {
        output->disabled = true;
        output->invalidate_after_disable = true;
        std::fprintf(stderr, "APPLaunch: framebuffer output disabled after an unsupported LVGL layout change\n");
    }
    if (output->disabled) {
        lv_display_flush_ready(display);
        return;
    }

    if (has_offset(display)) {
        // The driver can clip the shifted area with the original partial buffer layout.
        output->retry_queued = false;
        output->original_flush(display, area, pixels);
        return;
    }
    if (area->x1 < 0 || area->x2 >= kPanelWidth || area->y1 < 0 || area->y2 >= kPanelHeight) {
        lv_display_flush_ready(display);
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
        if (output->has_damage) {
            lv_area_join(&output->damaged_area, &output->damaged_area, area);
        } else {
            output->damaged_area = *area;
            output->has_damage = true;
            std::fprintf(stderr, "APPLaunch: framebuffer write failed (errno %d), scheduling redraw\n",
                         error);
        }
        output->failed_this_refresh = true;
    }
    lv_display_flush_ready(display);
}

void retry_failed_redraw(lv_display_t *display, StripeOutput &output)
{
    // REFR_READY comes after LVGL clears the invalidated areas for this frame.
    if (output.invalidate_after_disable) {
        output.invalidate_after_disable = false;
        const lv_area_t full{0, 0, lv_display_get_horizontal_resolution(display) - 1,
                            lv_display_get_vertical_resolution(display) - 1};
        lv_inv_area(display, &full);
    }
    if (output.disabled || !output.wrote_pixels) return;
    output.wrote_pixels = false;

    if (output.failed_this_refresh) {
        output.failed_this_refresh = false;
        output.retry_queued = false;
        if (output.retries < kMaxRedrawRetries) {
            output.retry_queued = lv_inv_area(display, &output.damaged_area) == LV_RESULT_OK;
            if (output.retry_queued) ++output.retries;
        } else if (output.retries == kMaxRedrawRetries) {
            ++output.retries;
            std::fprintf(stderr, "APPLaunch: framebuffer redraw retry limit reached\n");
        }
        return;
    }

    output.retries = 0;
    if (!output.has_damage) return;
    if (output.retry_queued) {
        output.retry_queued = false;
        output.has_damage = false;
    } else {
        // Writes have recovered elsewhere. Redraw the retained damage once.
        output.retry_queued = lv_inv_area(display, &output.damaged_area) == LV_RESULT_OK;
    }
}

void on_display_event(lv_event_t *event)
{
    auto *display = static_cast<lv_display_t *>(lv_event_get_target(event));
    auto *output = static_cast<StripeOutput *>(lv_event_get_user_data(event));
    switch (lv_event_get_code(event)) {
    case LV_EVENT_INVALIDATE_AREA: {
        if (output->disabled || has_offset(display) || !compatible_layout(display) ||
            !output->buffers_match(display)) break;
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
        !display->flush_cb || !display->buf_1 || !compatible_layout(display) || has_offset(display)) return;

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

    auto *output = new (std::nothrow) StripeOutput{fd, display->flush_cb, {{display->buf_1}, {display->buf_2}, {display->buf_3}}};
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
    if (!display || display->flush_cb != flush_stripe) return false;
    const auto *output = static_cast<StripeOutput *>(lv_display_get_user_data(display));
    return !output->disabled && compatible_layout(display) && !has_offset(display) &&
           output->buffers_match(display);
}

#else

void launcher_enable_fbdev_stripes() {}
bool launcher_fbdev_stripes_active() { return false; }

#endif
