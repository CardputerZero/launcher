/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_lvgl_app_page_assets.h"

#include "../assets/cp0_status_time_background.inc"
#include "../assets/cp0_status_time_background_wide.inc"
#include "../assets/cp0_status_battery_background.inc"
#include "../assets/setting_cross.inc"
#include "../assets/setting_ok.inc"
#include "../assets/setting_red_down.inc"
#include "../assets/setting_red_up.inc"
#include "../assets/setting_right_arrow.inc"

const lv_image_dsc_t cp0_status_time_background = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_ARGB8888,
        .flags = 0,
        .w = 40,
        .h = 16,
        .stride = 40 * 4,
    },
    .data_size = sizeof(cp0_status_time_background_map),
    .data = cp0_status_time_background_map,
};

/* Same pill, widened for the US 12-hour clock text ("h:MM AM/PM"). */
const lv_image_dsc_t cp0_status_time_background_wide = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_ARGB8888,
        .flags = 0,
        .w = 68,
        .h = 16,
        .stride = 68 * 4,
    },
    .data_size = sizeof(cp0_status_time_background_wide_map),
    .data = cp0_status_time_background_wide_map,
};

const lv_image_dsc_t cp0_status_battery_background = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_ARGB8888,
        .flags = 0,
        .w = 36,
        .h = 16,
        .stride = 36 * 4,
    },
    .data_size = sizeof(cp0_status_battery_background_map),
    .data = cp0_status_battery_background_map,
};
