#pragma once

#include "zclaw_process_runner.h"

#include <functional>
#include <string>
#include <vector>

namespace zclaw {

class ArchiveInstaller {
public:
    using Command = std::vector<std::string>;
    using CommandRunner = std::function<CommandResult(const Command &command)>;

    ArchiveInstaller();
    explicit ArchiveInstaller(CommandRunner command_runner);

    bool install_executable(const std::string &archive_path,
                            const std::string &work_directory,
                            const std::string &executable_name,
                            const std::string &destination,
                            std::string *error) const;

private:
    CommandRunner command_runner_;
};

}  // namespace zclaw
