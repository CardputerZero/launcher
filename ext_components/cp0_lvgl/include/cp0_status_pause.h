/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CP0_STATUS_PAUSE_H
#define CP0_STATUS_PAUSE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Set while the launcher's screen-off state holds the panel dark.  Periodic
 * UI refreshers (top-bar clock/status, battery events) and the network status
 * polling worker consult this and skip their work, so nothing redraws or
 * probes the system bus while the screen is off.  Wi-Fi itself stays
 * connected; the flag only pauses observation. */
void cp0_status_pause_set(int paused);
int cp0_status_pause_get(void);

#ifdef __cplusplus
}
#endif

#endif /* CP0_STATUS_PAUSE_H */
