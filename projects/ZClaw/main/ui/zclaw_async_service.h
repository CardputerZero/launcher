#pragma once

#include "zclaw_async_backend.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace zclaw {

class UiTaskSink;
class AsyncService {
public:
    using NetworkCallback = std::function<void(NetworkCheckResult)>;
    using ClientCallback = std::function<void(OperationResult)>;
    using ProgressCallback = std::function<void(SetupProgress)>;

    AsyncService(std::shared_ptr<UiTaskSink> tasks,
                 std::shared_ptr<AsyncBackend> backend);

    void shutdown() noexcept;

    bool check_network(NetworkCallback callback) const;
    bool send_chat(UiConfig config, std::string message,
                   AsyncBackend::ApprovalHandler approval_handler,
                   ClientCallback callback) const;
    bool pair(UiConfig config, std::string code, ClientCallback callback) const;
    bool setup(UiConfig config, ProviderConfig provider,
               ProgressCallback progress_callback,
               ClientCallback callback) const;

private:
    bool active() const;
    bool launch(std::function<void()> task) const;

    std::shared_ptr<UiTaskSink> tasks_;
    std::shared_ptr<AsyncBackend> backend_;
    mutable std::mutex mutex_;
    mutable bool shutdown_ = false;
};

}  // namespace zclaw
