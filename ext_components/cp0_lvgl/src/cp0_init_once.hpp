#pragma once

#include <mutex>
#include <utility>

namespace cp0 {

class InitOnce
{
public:
    template <typename Initializer>
    bool run(Initializer &&initializer)
    {
        try {
            std::call_once(flag_, [&] {
                if (!std::forward<Initializer>(initializer)()) throw Retry{};
            });
            return true;
        } catch (const Retry &) {
            return false;
        }
    }

private:
    struct Retry {};
    std::once_flag flag_;
};

} // namespace cp0
