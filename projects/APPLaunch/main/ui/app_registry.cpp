/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app_registry.h"

#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "model/app_registry_callback.hpp"

#include <charconv>
#include <cstdlib>
#include <string>

namespace {

AppRegistryCallbackSlot g_changed_callback;

int config_get_int(const char *key, int default_val)
{
    int val = default_val;
    cp0_signal_config_api({"GetInt", key ? std::string(key) : std::string(), std::to_string(default_val)},
                          [&](int code, std::string data) {
                              int parsed = 0;
                              const char *begin = data.data();
                              const char *end = begin + data.size();
                              const auto result = std::from_chars(begin, end, parsed);
                              if (code == 0 && result.ec == std::errc{} && result.ptr == end)
                                  val = parsed;
                          });
    return val;
}

bool config_set_int(const char *key, int val)
{
    bool succeeded = false;
    cp0_signal_config_api({"SetInt", key ? std::string(key) : std::string(), std::to_string(val)},
                          [&](int code, std::string) { succeeded = code == 0; });
    return succeeded;
}

bool config_save()
{
    bool succeeded = false;
    cp0_signal_config_api({"Save"},
                          [&](int code, std::string) { succeeded = code == 0; });
    return succeeded;
}

} // namespace

bool launcher_app_registry_is_enabled(const AppDescriptor &desc)
{
    if (desc.always_on || !desc.configurable)
        return true;
    if (!desc.config_key || !*desc.config_key) return true;
    return config_get_int(desc.config_key, 1) != 0;
}

bool launcher_app_registry_set_enabled(const AppDescriptor &desc, bool enabled)
{
    if (desc.always_on || !desc.configurable) return true;
    if (!desc.config_key || !*desc.config_key) return false;
    const int previous = config_get_int(desc.config_key, 1) != 0 ? 1 : 0;
    if (!config_set_int(desc.config_key, enabled ? 1 : 0)) return false;
    if (config_save()) return true;
    config_set_int(desc.config_key, previous);
    config_save();
    return false;
}

void launcher_app_registry_set_changed_callback(LauncherAppRegistryChangedCallback callback,
                                                void *user_data)
{
    g_changed_callback.set(callback, user_data);
}

bool launcher_app_registry_clear_changed_callback(LauncherAppRegistryChangedCallback callback,
                                                  void *user_data)
{
    return g_changed_callback.clear_if_matches(callback, user_data);
}

void launcher_app_registry_notify_changed()
{
    g_changed_callback.notify();
}
