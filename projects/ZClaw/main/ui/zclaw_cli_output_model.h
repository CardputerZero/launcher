#pragma once

#include <string>

namespace zclaw {

enum class CliServiceCommand {
    Install,
    Start,
};

bool cli_service_command_succeeded(bool command_ok, const std::string &output,
                                   CliServiceCommand command);
bool cli_agent_list_contains(const std::string &output, const std::string &alias);

}  // namespace zclaw
