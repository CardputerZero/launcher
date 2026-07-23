#pragma once

#include <list>
#include <string>

struct app;

void launcher_append_enabled_builtin_apps(std::list<app> &apps);
bool launcher_builtin_app_owns_exec(const std::string &exec);
