#include "zclaw_process_runner.h"

#include "zclaw_text.h"

#include <cstdio>

namespace zclaw {

bool CommandResult::ok() const
{
    return status == 0;
}

std::string ProcessRunner::shell_quote(const std::string &value)
{
    std::string output = "'";
    for (char character : value)
        output += character == '\'' ? "'\\''" : std::string(1, character);
    return output + "'";
}

CommandResult ProcessRunner::run_shell(const std::string &command)
{
    CommandResult result;
    FILE *pipe = ::popen((command + " 2>&1").c_str(), "r");
    if (!pipe) {
        result.output = "failed to start command";
        return result;
    }
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe))
        result.output += buffer;
    result.status = ::pclose(pipe);
    result.output = trim_ascii_whitespace(result.output);
    return result;
}

}  // namespace zclaw
