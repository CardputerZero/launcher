#pragma once

#include "zclaw_setup_service.h"

namespace zclaw {

class QuickstartBackend {
public:
    using ProgressHandler = SetupService::ProgressHandler;

    virtual ~QuickstartBackend() = default;

    virtual PreparedSetup prepare(UiConfig config, ProviderConfig provider,
                                  const ProgressHandler &progress_handler) const = 0;
    virtual OperationResult pair(UiConfig config,
                                 const std::string &pairing_code) const = 0;
};

}  // namespace zclaw
