#pragma once

#include <string>

namespace zclaw {

struct CommandResult {
    int status = -1;
    std::string output;

    bool ok() const;
};

class ProcessRunner {
public:
    static std::string shell_quote(const std::string &value);
    static CommandResult run_shell(const std::string &command);
};

}  // namespace zclaw
