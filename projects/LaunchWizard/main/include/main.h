#pragma once

#ifdef __cplusplus
int lvgl_main(void);
void ui_init(void);

// Returns true if the first-boot OOBE wizard should be shown on this device.
// Distinguishes a factory (unconfigured) image from a device the user already
// configured (e.g. CardputerZero Lite flashed with Raspberry Pi Imager).
bool launch_wizard_should_run(void);
int launch_wizard_finish_configured_system(void);

extern "C" {
#endif

#ifdef __cplusplus
}
#endif
