/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void cp0_keyboard_create_lvgl_input_devices(void);
void cp0_keyboard_dispatch_pending(void);

#ifdef __cplusplus
}
#endif
