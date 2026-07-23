#pragma once

#include <cstddef>
#include <list>

struct app;

inline constexpr std::size_t LAUNCHER_MAX_DESKTOP_APPS = 128;

void launcher_append_desktop_apps(std::list<app> &apps);
