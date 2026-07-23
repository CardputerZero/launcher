#pragma once

#include "zclaw_quickstart_backend.h"

#include <memory>

namespace zclaw {

class HttpCancellationRegistry;
class ProcessExecutor;
class LhvHttpClient;

class LocalQuickstartBackend final : public QuickstartBackend {
public:
    LocalQuickstartBackend() = default;
    explicit LocalQuickstartBackend(
        std::shared_ptr<HttpCancellationRegistry> cancellation,
        std::shared_ptr<ProcessExecutor> processes = nullptr,
        std::shared_ptr<LhvHttpClient> http_client = nullptr);

    PreparedSetup prepare(UiConfig config, ProviderConfig provider,
                          const ProgressHandler &progress_handler) const override;
    OperationResult pair(UiConfig config,
                         const std::string &pairing_code) const override;

private:
    std::shared_ptr<HttpCancellationRegistry> cancellation_;
    std::shared_ptr<ProcessExecutor> processes_;
    std::shared_ptr<LhvHttpClient> http_client_;
};

}  // namespace zclaw
