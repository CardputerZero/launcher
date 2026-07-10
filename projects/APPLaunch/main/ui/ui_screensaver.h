/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct key_item;

void ui_screensaver_init(void);
int ui_screensaver_filter_key(const struct key_item *item);
void ui_screensaver_set_foreground(int foreground);

#ifdef __cplusplus
}
#endif
