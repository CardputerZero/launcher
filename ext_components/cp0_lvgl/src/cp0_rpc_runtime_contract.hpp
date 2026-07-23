#pragma once

#include <utility>

namespace cp0::rpc {

class RuntimeState {
public:
    enum class Phase { Idle, Running, Stopping };

    bool can_start() const noexcept { return phase_ == Phase::Idle; }
    bool running() const noexcept { return phase_ == Phase::Running; }
    bool stopping() const noexcept { return phase_ == Phase::Stopping; }
    void mark_started() noexcept { phase_ = Phase::Running; }
    void mark_stopping() noexcept { phase_ = Phase::Stopping; }
    void mark_stopped() noexcept { phase_ = Phase::Idle; }

private:
    Phase phase_ = Phase::Idle;
};

template <typename Future>
bool await_readiness(Future &future) noexcept
{
    try {
        return future.get();
    } catch (...) {
        return false;
    }
}

struct ShutdownResult {
    bool context_shutdown = true;
    bool subscriber_joined = true;
    bool broker_joined = true;

    bool workers_joined() const noexcept
    {
        return subscriber_joined && broker_joined;
    }
};

template <typename MarkStopping, typename ShutdownContext,
          typename JoinSubscriber, typename JoinBroker>
ShutdownResult shutdown_runtime(MarkStopping &&mark_stopping,
                      ShutdownContext &&shutdown_context,
                      JoinSubscriber &&join_subscriber,
                      JoinBroker &&join_broker) noexcept
{
    ShutdownResult result;
    try { std::forward<MarkStopping>(mark_stopping)(); } catch (...) {}
    try { std::forward<ShutdownContext>(shutdown_context)(); }
    catch (...) { result.context_shutdown = false; }
    try { std::forward<JoinSubscriber>(join_subscriber)(); }
    catch (...) { result.subscriber_joined = false; }
    try { std::forward<JoinBroker>(join_broker)(); }
    catch (...) { result.broker_joined = false; }
    return result;
}

} // namespace cp0::rpc
