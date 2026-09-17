# cp0_lvgl Work Entry

## Read First

- Public interfaces, all `cp0_signal_*` commands, arguments, callback results, and thread rules: read [`docs/cp0_lvgl.en.md`](../../docs/cp0_lvgl.en.md).
- Chinese and Japanese references: [`docs/cp0_lvgl.md`](../../docs/cp0_lvgl.md) and [`docs/cp0_lvgl.ja.md`](../../docs/cp0_lvgl.ja.md).
- Authoritative signal signatures: `include/signal_register_plan.h`.
- C ABI: `include/cp0_lvgl_app.h`; LVGL pages and runner: `include/cp0_lvgl_app_runner.hpp` and `include/ui_app_page.hpp`.
- The README files are call-site references. When fixing a bug, changing behavior, or checking a platform difference, inspect the relevant implementation under `src/` and its `*_contract.*` files, then compare the device and SDL backends.

## Module Boundary

`cp0_lvgl` provides the LVGL runner, reusable page bases, and eventpp callback-list services for audio, PTY, configuration, filesystem, LoRa, Wi-Fi, Bluetooth, settings, process execution, OS information, time, battery, screenshots, camera, soundcard, and sudo. Whether a service is installed is controlled by `CONFIG_CP0_LVGL_INIT_*`; never assume a disabled service exists.

Request signals normally take `std::list<std::string>` with the command name at element zero and a `std::function<void(int, std::string)>` callback. `code == 0` means success; interpret `data` using the wire format documented in the README. Callbacks may run on worker threads, so UI code must marshal back to the LVGL thread. Stop background threads, timers, PTYs, camera work, and sudo requests before service teardown.

## Public Interface Quick Reference

- Runner: `cp0_lvgl_init()`, `cp0_lvgl_run()`, `cp0_lvgl_wake()`; pages: `AppPageRoot`, `AppPage`, `AppPageWithBottomBarLayout`, `cp0_lvgl_start_app_page()`, and `cp0_lvgl_start_app<PageT>()`.
- C wrappers: `cp0_file_*` / `cp0_dir_*`, `cp0_network_list`, `cp0_wifi_*`, `cp0_process_*`, `cp0_system_shutdown/reboot`, `cp0_sudo_*`, `cp0_battery_read`, `cp0_bq27220_calibrate`, `cp0_backlight_*`, `cp0_time_*`, `cp0_eth_info_read`, `cp0_account_info_read`, and background update functions. Full declarations are in `include/cp0_lvgl_app.h`.
- Request/response signals: `cp0_signal_audio_api`, `cp0_signal_audio_setup`, `cp0_signal_pty_api`, `cp0_signal_config_api`, `cp0_signal_filesystem_api`, `cp0_signal_lora_api`, `cp0_signal_wifi_api`, `cp0_signal_bt_api`, `cp0_signal_settings_api`, `cp0_signal_process_api`, `cp0_signal_osinfo_api`, `cp0_signal_timedate_api`, `cp0_signal_bq27220_api`, `cp0_signal_screenshot_api`, `cp0_signal_camera_api`, and `cp0_signal_soundcard_api`.
- One-way/event signals: `cp0_signal_audio_play(std::string)`, `cp0_signal_audio_cap(bool)`, `cp0_signal_system_play(std::string)`, `cp0_signal_battery_pub(std::function<void()>)`, `cp0_signal_network()`, and `cp0_signal_forkexec()`.
- Async-specialized signals: `cp0_signal_bt_agent(uint64_t, method, device, value, reply)`, `cp0_signal_sudo_argv_async(args, auth_timeout_ms, exec_timeout_ms, complete, started)`, `cp0_signal_sudo_cancel(request_id, done)`, and `cp0_signal_system_admin_async(args, auth_timeout_ms, exec_timeout_ms, complete, started)`.
- `cp0_signal_network` and `cp0_signal_forkexec` currently have only plan declarations and no `cp0_lvgl` registration. Confirm an implementation exists before calling them. Bluetooth Agent requests originate on a worker thread; sudo calls `started` before `complete`; neither callback may directly manipulate LVGL objects.

For command details, argument counts, return encoding, and platform differences, use the “Complete `cp0_signal_*` Usage” section in [`docs/cp0_lvgl.en.md`](../../docs/cp0_lvgl.en.md).

## Modification Rules

- A new signal exposed to external modules must be declared in `include/signal_register_plan.h` with `def_hal_fun`, and must include registration, teardown, device and SDL behavior, and tests. Example:

  `def_hal_fun(void(std::list<std::string>, std::function<void(int, std::string)>), cp0_signal_xxx_api)`

- When changing an argument or return format, update the English, Chinese, and Japanese documents in `docs/`, the corresponding contract, C wrappers, all call sites, and tests.
- Do not add new uses of deprecated `cp0_process_run_sudo` or `cp0_signal_process_api` `RunSudo`; use the asynchronous sudo signals.
- Run `ext_components/cp0_lvgl/tests/run_tests.sh` for module regression. Changes involving Linux commands, BlueZ, libcamera, framebuffer, or sudo also require target-platform validation.
