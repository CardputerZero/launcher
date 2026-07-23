#pragma once

#include "cp0_lvgl_app.h"

namespace cp0_bluez_dbus {

cp0_bt_status_t status();
int set_power(int enabled);
int set_alias(const char *alias);
int set_discoverable(int enabled);
int start_discovery();
int stop_discovery();
int list(cp0_bt_device_t *out, int max_devices, bool connected_only);
int pair(const char *address);
int connect(const char *address);
int disconnect(const char *address);
int remove(const char *address);

} // namespace cp0_bluez_dbus
