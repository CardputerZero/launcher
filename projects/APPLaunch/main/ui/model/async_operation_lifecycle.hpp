#pragma once

#include <cstdint>
#include <memory>
#include <mutex>

namespace setting {

class AsyncOperationLifecycle
{
private:
    struct State;

public:
    class Token
    {
    public:
        Token() = default;

        explicit operator bool() const { return generation_ != 0 && !state_.expired(); }
        bool current() const;
        bool complete() const;

    private:
        friend class AsyncOperationLifecycle;
        Token(std::weak_ptr<State> state, uint64_t generation)
            : state_(std::move(state)), generation_(generation)
        {
        }

        std::weak_ptr<State> state_;
        uint64_t generation_ = 0;
    };

    AsyncOperationLifecycle();
    ~AsyncOperationLifecycle();

    AsyncOperationLifecycle(const AsyncOperationLifecycle &) = delete;
    AsyncOperationLifecycle &operator=(const AsyncOperationLifecycle &) = delete;

    Token begin();
    bool activate(const Token &token, uint64_t request_id);
    bool abort(const Token &token);
    bool active() const;
    uint64_t shutdown();

private:
    struct State
    {
        mutable std::mutex mutex;
        bool alive = true;
        bool active = false;
        uint64_t generation = 0;
        uint64_t request_id = 0;
    };

    std::shared_ptr<State> state_;
};

} // namespace setting
