#pragma once

#include <functional>
#include <string>
#include <utility>

namespace cp0 {

class CallbackResult
{
public:
    using Callback = std::function<void(int, std::string)>;

    explicit CallbackResult(Callback callback) : callback_(std::move(callback)) {}

    bool complete(int code, const std::string &data) noexcept
    {
        if (completed_) return false;
        completed_ = true;
        if (!callback_) return true;
        try {
            callback_(code, data);
        } catch (...) {
        }
        return true;
    }

    bool completed() const noexcept { return completed_; }

    template <typename Operation>
    void guard(int failure_code, const std::string &failure_data,
               Operation &&operation) noexcept
    {
        try {
            std::forward<Operation>(operation)();
        } catch (...) {
            complete(failure_code, failure_data);
        }
    }

private:
    Callback callback_;
    bool completed_ = false;
};

} // namespace cp0
