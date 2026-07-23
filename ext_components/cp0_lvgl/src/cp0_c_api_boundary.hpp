#pragma once

#include <utility>

namespace cp0 {

template <typename Result, typename Operation>
Result invoke_c_api_or(Result fallback, Operation &&operation) noexcept
{
    try {
        return std::forward<Operation>(operation)();
    } catch (...) {
        return fallback;
    }
}

} // namespace cp0
