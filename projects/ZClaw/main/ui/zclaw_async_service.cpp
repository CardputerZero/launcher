#include "zclaw_async_service.h"

#include "zclaw_ui_task_sink.h"

#include <hv/hasync.h>

#include <exception>
#include <stdexcept>
#include <utility>

namespace zclaw {
namespace {

std::string exception_detail()
{
    try {
        throw;
    } catch (const std::exception &error) {
        return error.what();
    } catch (...) {
        return "unknown exception";
    }
}

template <typename Call>
OperationResult call_operation(UiConfig fallback_config, const char *operation,
                               Call call)
{
    try {
        return call();
    } catch (...) {
        return {std::string(operation) + " failed.\nUnexpected backend error: " +
                    exception_detail(),
                false, std::move(fallback_config)};
    }
}

}  // namespace

AsyncService::AsyncService(std::shared_ptr<UiTaskSink> tasks,
                           std::shared_ptr<AsyncBackend> backend)
    : tasks_(std::move(tasks)), backend_(std::move(backend))
{
}

void AsyncService::shutdown() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_)
        return;
    shutdown_ = true;
    if (backend_)
        backend_->shutdown();
}

bool AsyncService::launch(std::function<void()> task) const
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_ || !task)
            return false;
    }
    try {
        (void)hv::async(std::move(task));
        return true;
    } catch (...) {
        return false;
    }
}

bool AsyncService::active() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !shutdown_;
}

bool AsyncService::check_network(NetworkCallback callback) const
{
    if (!callback || !tasks_ || !backend_ || !active())
        return false;
    const std::shared_ptr<UiTaskSink> tasks = tasks_;
    const std::shared_ptr<AsyncBackend> backend = backend_;
    auto completion = [tasks, callback = std::move(callback)](
                          NetworkCheckResult result) mutable {
        tasks->post([callback = std::move(callback), result = std::move(result)]() mutable {
            callback(std::move(result));
        });
    };
    try {
        return backend->check_network(completion);
    } catch (...) {
        NetworkCheckResult result;
        result.error = "Network check failed.\nUnexpected backend error: " +
                       exception_detail();
        completion(std::move(result));
        return true;
    }
}

bool AsyncService::send_chat(UiConfig config, std::string message,
                             AsyncBackend::ApprovalHandler approval_handler,
                             ClientCallback callback) const
{
    if (!callback || !tasks_ || !backend_ || !active())
        return false;
    const std::shared_ptr<UiTaskSink> tasks = tasks_;
    const std::shared_ptr<AsyncBackend> backend = backend_;
    UiConfig fallback_config = config;
    auto completion = [tasks, callback = std::move(callback)](
                          OperationResult result) mutable {
        tasks->post([callback = std::move(callback), result = std::move(result)]() mutable {
            callback(std::move(result));
        });
    };
    try {
        return backend->send_chat(std::move(config), std::move(message),
                                  std::move(approval_handler),
                                  completion);
    } catch (...) {
        completion({"Chat request failed.\nUnexpected backend error: " +
                        exception_detail(),
                    false, std::move(fallback_config)});
        return true;
    }
}

bool AsyncService::pair(UiConfig config, std::string code,
                        ClientCallback callback) const
{
    if (!callback || !tasks_ || !backend_ || !active())
        return false;
    const std::shared_ptr<UiTaskSink> tasks = tasks_;
    const std::shared_ptr<AsyncBackend> backend = backend_;
    UiConfig fallback_config = config;
    auto completion = [tasks, callback = std::move(callback)](
                          OperationResult result) mutable {
        tasks->post([callback = std::move(callback), result = std::move(result)]() mutable {
            callback(std::move(result));
        });
    };
    try {
        return backend->pair(std::move(config), std::move(code),
                             completion);
    } catch (...) {
        completion({"Pairing request failed.\nUnexpected backend error: " +
                        exception_detail(),
                    false, std::move(fallback_config)});
        return true;
    }
}

bool AsyncService::setup(UiConfig config, ProviderConfig provider,
                         ProgressCallback progress_callback,
                         ClientCallback callback) const
{
    if (!callback || !tasks_ || !backend_ || !active())
        return false;
    const std::shared_ptr<UiTaskSink> tasks = tasks_;
    const std::shared_ptr<AsyncBackend> backend = backend_;
    return launch(
        [tasks, backend, config = std::move(config), provider = std::move(provider),
         progress_callback = std::move(progress_callback),
         callback = std::move(callback)]() mutable {
            UiConfig fallback_config = config;
            OperationResult result = call_operation(
                std::move(fallback_config), "Setup",
                [&] {
                    return backend->setup(
                        std::move(config), std::move(provider),
                        [tasks, progress_callback](const SetupProgress &progress) {
                            if (progress_callback)
                                tasks->post([progress_callback, progress] {
                                    progress_callback(progress);
                                });
                        });
                });
            tasks->post(
                [callback = std::move(callback), result = std::move(result)]() mutable {
                    callback(std::move(result));
                });
        });
}

}  // namespace zclaw
