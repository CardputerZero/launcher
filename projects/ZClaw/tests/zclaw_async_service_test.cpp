#include "zclaw_async_backend.h"
#include "zclaw_async_service.h"
#include "zclaw_ui_task_sink.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeTaskSink : public zclaw::UiTaskSink {
public:
    bool post(Task task) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push_back(std::move(task));
        changed_.notify_all();
        return true;
    }

    bool wait_for_count(std::size_t count)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(1),
                                 [this, count] { return tasks_.size() >= count; });
    }

    void drain()
    {
        std::vector<Task> tasks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks.swap(tasks_);
        }
        for (Task &task : tasks)
            task();
    }

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<Task> tasks_;
};

class FakeAsyncBackend : public zclaw::AsyncBackend {
public:
    void shutdown() noexcept override
    {
        shutdown_called = true;
    }

    bool check_network(NetworkCompletion completion) const override
    {
        completion({true, ""});
        return true;
    }

    bool send_chat(
        zclaw::UiConfig config, std::string message,
        ApprovalHandler, OperationCompletion completion) const override
    {
        completion({"chat:" + message, true, std::move(config)});
        return true;
    }

    bool pair(zclaw::UiConfig config, std::string code,
              OperationCompletion completion) const override
    {
        completion({"pair:" + code, true, std::move(config)});
        return true;
    }

    zclaw::OperationResult setup(
        zclaw::UiConfig config, zclaw::ProviderConfig,
        const ProgressHandler &progress_handler) const override
    {
        progress_handler({"Halfway", 50});
        return {"setup complete", true, std::move(config)};
    }

    std::atomic<bool> shutdown_called{false};
};

class ThrowingAsyncBackend : public zclaw::AsyncBackend {
public:
    bool check_network(NetworkCompletion) const override
    {
        throw std::runtime_error("network exploded");
    }

    bool send_chat(zclaw::UiConfig, std::string, ApprovalHandler,
                   OperationCompletion) const override
    {
        throw std::runtime_error("chat exploded");
    }

    bool pair(zclaw::UiConfig, std::string,
              OperationCompletion) const override
    {
        throw 42;
    }

    zclaw::OperationResult setup(
        zclaw::UiConfig, zclaw::ProviderConfig,
        const ProgressHandler &) const override
    {
        throw std::runtime_error("setup exploded");
    }
};

}  // namespace

int main()
{
    auto tasks = std::make_shared<FakeTaskSink>();
    auto backend = std::make_shared<FakeAsyncBackend>();
    zclaw::AsyncService service(tasks, backend);

    bool network_called = false;
    assert(service.check_network([&network_called](zclaw::NetworkCheckResult result) {
        assert(result.online);
        network_called = true;
    }));
    assert(tasks->wait_for_count(1));
    assert(!network_called);
    tasks->drain();
    assert(network_called);

    std::string chat_result;
    assert(service.send_chat({}, "hello", {},
                             [&chat_result](zclaw::OperationResult result) {
                                 chat_result = result.text;
                             }));
    assert(tasks->wait_for_count(1));
    assert(chat_result.empty());
    tasks->drain();
    assert(chat_result == "chat:hello");

    std::vector<std::string> setup_events;
    assert(service.setup(
        {}, {},
        [&setup_events](zclaw::SetupProgress progress) {
            setup_events.push_back("progress:" + std::to_string(progress.percent));
        },
        [&setup_events](zclaw::OperationResult result) {
            setup_events.push_back(result.text);
        }));
    assert(tasks->wait_for_count(2));
    assert(setup_events.empty());
    tasks->drain();
    assert((setup_events ==
            std::vector<std::string>{"progress:50", "setup complete"}));

    assert(!service.check_network({}));
    assert(!service.pair({}, "code", {}));
    zclaw::AsyncService missing_tasks(nullptr, backend);
    assert(!missing_tasks.send_chat({}, "hello", {},
                                    [](zclaw::OperationResult) {}));

    auto lifetime_tasks = std::make_shared<FakeTaskSink>();
    auto lifetime_backend = std::make_shared<FakeAsyncBackend>();
    std::string lifetime_result;
    {
        zclaw::AsyncService transient(lifetime_tasks, lifetime_backend);
        assert(transient.send_chat(
            {}, "after-destruction", {},
            [&lifetime_result](zclaw::OperationResult result) {
                lifetime_result = result.text;
            }));
    }
    lifetime_backend.reset();
    assert(lifetime_tasks->wait_for_count(1));
    lifetime_tasks->drain();
    assert(lifetime_result == "chat:after-destruction");

    zclaw::AsyncService missing_backend(tasks, nullptr);
    assert(!missing_backend.check_network([](zclaw::NetworkCheckResult) {}));

    auto throwing_tasks = std::make_shared<FakeTaskSink>();
    zclaw::AsyncService throwing_service(
        throwing_tasks, std::make_shared<ThrowingAsyncBackend>());
    std::vector<std::string> failures;
    zclaw::UiConfig fallback_config;
    fallback_config.agent_alias = "preserved";
    assert(throwing_service.check_network(
        [&failures](zclaw::NetworkCheckResult result) {
            assert(!result.online);
            failures.push_back(result.error);
        }));
    assert(throwing_service.send_chat(
        fallback_config, "hello", {},
        [&failures](zclaw::OperationResult result) {
            assert(!result.ok && result.config.agent_alias == "preserved");
            failures.push_back(result.text);
        }));
    assert(throwing_service.pair(
        fallback_config, "code", [&failures](zclaw::OperationResult result) {
            assert(!result.ok && result.config.agent_alias == "preserved");
            failures.push_back(result.text);
        }));
    assert(throwing_service.setup(
        fallback_config, {}, {},
        [&failures](zclaw::OperationResult result) {
            assert(!result.ok && result.config.agent_alias == "preserved");
            failures.push_back(result.text);
        }));
    assert(throwing_tasks->wait_for_count(4));
    assert(failures.empty());
    throwing_tasks->drain();
    assert(failures.size() == 4);
    assert(failures[0].find("network exploded") != std::string::npos);
    assert(failures[1].find("chat exploded") != std::string::npos);
    assert(failures[2].find("unknown exception") != std::string::npos);
    assert(failures[3].find("setup exploded") != std::string::npos);
    throwing_service.shutdown();

    service.shutdown();
    service.shutdown();
    assert(backend->shutdown_called);
    assert(!service.check_network([](zclaw::NetworkCheckResult) {}));
    return 0;
}
