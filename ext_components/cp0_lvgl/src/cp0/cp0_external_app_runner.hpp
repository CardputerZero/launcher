#pragma once

namespace cp0_external_app_runner {

int run(const char *command, volatile int *home_key_flag, bool keep_root);

} // namespace cp0_external_app_runner
