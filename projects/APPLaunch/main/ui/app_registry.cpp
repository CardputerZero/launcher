/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app_registry.h"

#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <cstdlib>
#include <string>

namespace {

LauncherAppRegistryChangedCallback g_changed_callback = nullptr;
void *g_changed_user_data = nullptr;

int config_get_int(const char *key, int default_val)
{
    int val = default_val;
    cp0_signal_config_api({"GetInt", key ? std::string(key) : std::string(), std::to_string(default_val)},
                          [&](int code, std::string data) {
                              if (code == 0) val = std::atoi(data.c_str());
                          });
    return val;
}

void config_set_int(const char *key, int val)
{
    cp0_signal_config_api({"SetInt", key ? std::string(key) : std::string(), std::to_string(val)}, nullptr);
}

} // namespace

bool launcher_app_registry_is_enabled(const AppDescriptor &desc)
{
    if (desc.always_on || !desc.configurable)
        return true;
    return config_get_int(desc.config_key, 1) != 0;
}

void launcher_app_registry_set_enabled(const AppDescriptor &desc, bool enabled)
{
    if (desc.always_on || !desc.configurable)
        enabled = true;
    config_set_int(desc.config_key, enabled ? 1 : 0);
}

void launcher_app_registry_set_changed_callback(LauncherAppRegistryChangedCallback callback,
                                                void *user_data)
{
    g_changed_callback = callback;
    g_changed_user_data = user_data;
}

void launcher_app_registry_notify_changed()
{
    if (g_changed_callback)
        g_changed_callback(g_changed_user_data);
}
