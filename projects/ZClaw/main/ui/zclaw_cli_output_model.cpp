#include "zclaw_cli_output_model.h"

#include "zclaw_text.h"

#include <sstream>

namespace zclaw {
namespace {

bool contains(const std::string &value, const char *marker)
{
    return value.find(marker) != std::string::npos;
}

}  // namespace

bool cli_service_command_succeeded(bool command_ok, const std::string &output,
                                   CliServiceCommand command)
{
    if (command_ok)
        return true;
    if (contains(output, "already"))
        return true;
    if (command == CliServiceCommand::Install)
        return contains(output, "exists");
    return contains(output, "active");
}

bool cli_agent_list_contains(const std::string &output, const std::string &alias)
{
    std::istringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
        if (trim_ascii_whitespace(line) == alias)
            return true;
    }
    return false;
}

}  // namespace zclaw
