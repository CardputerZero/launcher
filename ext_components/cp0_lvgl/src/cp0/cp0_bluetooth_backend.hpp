#pragma once

#include "cp0_lvgl_app.h"

namespace cp0_bluetooth_backend {

cp0_bt_status_t status();
int set_power(int on);
int set_alias(const char *alias);
int set_discoverable(int on);
int start_discovery();
int stop_discovery();
int scan(cp0_bt_device_t *out, int max_devices);
int list(cp0_bt_device_t *out, int max_devices, bool connected_only);
int pair(const char *address);
int connect(const char *address);
int disconnect(const char *address);
int remove(const char *address);

} // namespace cp0_bluetooth_backend
