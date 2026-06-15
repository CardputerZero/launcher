/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

struct AppDescriptor {
    const char *label;
    const char *icon;
    const char *config_key;
    bool configurable;
    bool always_on;
};

const AppDescriptor *launcher_app_registry_entries(std::size_t *count);
bool launcher_app_registry_is_enabled(const AppDescriptor &desc);
void launcher_app_registry_set_enabled(const AppDescriptor &desc, bool enabled);

typedef void (*LauncherAppRegistryChangedCallback)(void *user_data);
void launcher_app_registry_set_changed_callback(LauncherAppRegistryChangedCallback callback,
                                                void *user_data);
void launcher_app_registry_notify_changed();
