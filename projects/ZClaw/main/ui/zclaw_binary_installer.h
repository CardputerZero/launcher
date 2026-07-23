#pragma once

#include "zclaw_types.h"

#include <functional>
#include <memory>
#include <string>

namespace zclaw {

class BinaryInstallerBackend;

class BinaryInstaller {
public:
    using ProgressHandler = std::function<void(const SetupProgress &)>;

    explicit BinaryInstaller(std::shared_ptr<BinaryInstallerBackend> backend);

    bool ensure_installed(std::string *error,
                          const ProgressHandler &progress_handler) const;

private:
    std::shared_ptr<BinaryInstallerBackend> backend_;
};

}  // namespace zclaw
