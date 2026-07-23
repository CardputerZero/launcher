#pragma once

#include "cp0_lvgl_app.h"

namespace cp0_process_lifecycle {

cp0_pid_t spawn(const char *command, bool keep_root);
void stop(cp0_pid_t pid);
int check_lock(const char *path, int *holder_pid);
void kill(int pid, int grace_ms);

} // namespace cp0_process_lifecycle
