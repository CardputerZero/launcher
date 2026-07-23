#pragma once

#include "zclaw_types.h"

#include <functional>

namespace zclaw {

class QuickstartBackend;

class QuickstartService {
public:
    using ProgressHandler = std::function<void(const SetupProgress &)>;

    explicit QuickstartService(const QuickstartBackend &backend);

    OperationResult run(UiConfig config, const ProviderConfig &provider,
                        const ProgressHandler &progress_handler = nullptr) const;

private:
    const QuickstartBackend *backend_ = nullptr;
};

}  // namespace zclaw
