#pragma once

#include <string>
#include <vector>

namespace cp0_process_commands {

int run_argv(const std::vector<std::string> &argv, bool background);
int run_sudo(const std::string &password, const std::vector<std::string> &argv);
int capture_argv(const std::vector<std::string> &argv, std::string &output);

// Replaces the child process image and only returns on unsupported platforms.
void exec_shell_as_configured_user(const char *command);

} // namespace cp0_process_commands
