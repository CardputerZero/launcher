#pragma once

#include "zclaw_types.h"

#include <functional>
#include <string>

namespace zclaw {

class SetupBackend;

struct PreparedSetup {
    UiConfig config;
    std::string pairing_code;
    std::string error;
    bool ok = false;
};

class SetupService {
public:
    using ProgressHandler = std::function<void(const SetupProgress &)>;

    explicit SetupService(const SetupBackend &backend);

    PreparedSetup prepare(UiConfig config, ProviderConfig provider,
                          const ProgressHandler &progress_handler) const;

private:
    const SetupBackend *backend_ = nullptr;
};

}  // namespace zclaw
