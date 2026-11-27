/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_status_pause.h"

/* Plain volatile int on purpose: single-word flag, set from the LVGL thread
 * (screensaver) and read from the LVGL thread plus the network polling
 * worker.  Same pattern as LVGL_RUN_FLAGE. */
static volatile int s_status_pause = 0;

void cp0_status_pause_set(int paused)
{
    s_status_pause = paused ? 1 : 0;
}

int cp0_status_pause_get(void)
{
    return s_status_pause;
}
