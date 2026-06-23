/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * ui_darkscreen.h
 *
 * "DarkTime" idle screen blanking for the launcher (#72).
 *
 * After the configured idle timeout ("dark_time" seconds, 0 = Never) with no
 * key activity, the launcher turns the backlight off AND paints a full-screen
 * black overlay so the panel is fully dark. Any key press wakes it again; that
 * waking key is swallowed so it does not also trigger an action.
 *
 * Only active while the launcher is the foreground process
 * (LVGL_RUN_FLAGE == 1); sub-apps are unaffected.
 */

#ifndef UI_DARKSCREEN_H
#define UI_DARKSCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

struct key_item;

/* Call once per main-loop iteration. Blanks the screen after the configured
 * idle timeout while the launcher is in the foreground. */
void ui_darkscreen_tick(void);

/* Per-key hook, invoked from the central keyboard dispatch BEFORE the key is
 * delivered to the UI. Resets the idle timer on every key. Returns 1 if the
 * key was swallowed (used only to wake the screen) and must NOT reach the UI;
 * returns 0 otherwise. */
int ui_darkscreen_filter_key(const struct key_item *elm);

#ifdef __cplusplus
}
#endif

#endif /* UI_DARKSCREEN_H */
