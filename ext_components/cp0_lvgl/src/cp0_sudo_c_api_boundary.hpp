#pragma once

#include <cerrno>
#include <new>
#include <utility>

namespace cp0_sudo {

template <typename Operation>
int invoke_c_api(Operation &&operation) noexcept
{
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc &) {
        return -ENOMEM;
    } catch (...) {
        return -EIO;
    }
}

} // namespace cp0_sudo
