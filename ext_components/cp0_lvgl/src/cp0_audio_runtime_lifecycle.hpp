#pragma once

namespace cp0::audio {

class RuntimeLifecycle
{
public:
    bool begin_init() noexcept
    {
        if (state_ != State::INACTIVE) return false;
        state_ = State::INITIALIZING;
        return true;
    }

    void commit_init() noexcept
    {
        if (state_ == State::INITIALIZING) state_ = State::ACTIVE;
    }

    void rollback_init() noexcept
    {
        if (state_ == State::INITIALIZING) state_ = State::INACTIVE;
    }

    bool begin_shutdown() noexcept
    {
        if (state_ == State::INACTIVE) return false;
        state_ = State::INACTIVE;
        return true;
    }

    bool active() const noexcept { return state_ == State::ACTIVE; }

private:
    enum class State { INACTIVE, INITIALIZING, ACTIVE };
    State state_ = State::INACTIVE;
};

} // namespace cp0::audio
