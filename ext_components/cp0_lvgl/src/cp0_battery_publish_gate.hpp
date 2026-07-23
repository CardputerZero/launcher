#pragma once

#include <atomic>
#include <utility>

namespace cp0::battery {

class PublishGate
{
public:
    template <typename Operation>
    bool run(Operation &&operation) noexcept
    {
        bool expected = false;
        if (!active_.compare_exchange_strong(expected, true)) return false;
        try {
            std::forward<Operation>(operation)();
        } catch (...) {
        }
        active_.store(false);
        return true;
    }

private:
    std::atomic<bool> active_{false};
};

} // namespace cp0::battery
