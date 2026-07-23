#pragma once

#include "zclaw_types.h"

#include <functional>
#include <string>

namespace zclaw {

struct NetworkCheckResult {
    bool online = false;
    std::string error;
};

class AsyncBackend {
public:
    using ApprovalDecision = std::function<void(std::string)>;
    using ApprovalHandler = std::function<void(
        const ApprovalRequest &, ApprovalDecision)>;
    using ProgressHandler = std::function<void(const SetupProgress &)>;
    using NetworkCompletion = std::function<void(NetworkCheckResult)>;
    using OperationCompletion = std::function<void(OperationResult)>;

    virtual ~AsyncBackend() = default;

    // Requests cancellation where supported. Other operations may still run to
    // completion and are joined by AsyncService::shutdown().
    virtual void shutdown() noexcept {}
    virtual bool check_network(NetworkCompletion completion) const = 0;
    virtual bool send_chat(
        UiConfig config, std::string message,
        ApprovalHandler approval_handler,
        OperationCompletion completion) const = 0;
    virtual bool pair(UiConfig config, std::string code,
                      OperationCompletion completion) const = 0;
    virtual OperationResult setup(
        UiConfig config, ProviderConfig provider,
        const ProgressHandler &progress_handler) const = 0;
};

}  // namespace zclaw
