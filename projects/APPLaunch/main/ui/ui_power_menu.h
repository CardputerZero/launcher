/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/* Power menu shown over the launcher home screen after ESC is held for 3s:
 * Cancel / Lock / Reboot / Shutdown, each action confirmed with OK.
 * LVGL thread only. */
namespace ui_power_menu {

/* Installs the key filter that owns input while the menu is open. */
void init();
void shutdown();

bool open();
bool is_open();

} // namespace ui_power_menu
