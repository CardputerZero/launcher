#pragma once

#include "zclaw_async_backend.h"
#include "zclaw_connectivity.h"

#include <memory>

namespace zclaw {

class HttpCancellationRegistry;
class ProcessExecutor;
class LhvHttpClient;

class LocalAsyncBackend final : public AsyncBackend {
public:
    LocalAsyncBackend();
    explicit LocalAsyncBackend(std::shared_ptr<ConnectivityProbe> probe);

    void shutdown() noexcept override;
    bool check_network(NetworkCompletion completion) const override;
    bool send_chat(
        UiConfig config, std::string message,
        ApprovalHandler approval_handler,
        OperationCompletion completion) const override;
    bool pair(UiConfig config, std::string code,
              OperationCompletion completion) const override;
    OperationResult setup(
        UiConfig config, ProviderConfig provider,
        const ProgressHandler &progress_handler) const override;

private:
    std::shared_ptr<HttpCancellationRegistry> cancellation_;
    std::shared_ptr<ProcessExecutor> processes_;
    std::shared_ptr<LhvHttpClient> http_client_;
    mutable ConnectivityChecker connectivity_;
};

}  // namespace zclaw
