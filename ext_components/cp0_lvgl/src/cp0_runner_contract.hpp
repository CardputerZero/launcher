#pragma once

#include <atomic>
#include <utility>

namespace cp0 {

class RunnerLifetime
{
public:
    bool claim()
    {
        State expected = State::Fresh;
        return state_.compare_exchange_strong(
            expected, State::Claimed,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }

    void initialization_failed()
    {
        State expected = State::Claimed;
        state_.compare_exchange_strong(
            expected, State::Fresh,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }

    void finish()
    {
        State expected = State::Claimed;
        state_.compare_exchange_strong(
            expected, State::Finished,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }

private:
    enum class State { Fresh, Claimed, Finished };
    std::atomic<State> state_{State::Fresh};
};

template <typename Operation>
void invoke_resume_callback(Operation &&operation) noexcept
{
    try {
        std::forward<Operation>(operation)();
    } catch (...) {
    }
}

} // namespace cp0
