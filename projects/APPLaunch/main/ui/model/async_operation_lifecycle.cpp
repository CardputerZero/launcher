#include "async_operation_lifecycle.hpp"

namespace setting {

AsyncOperationLifecycle::AsyncOperationLifecycle() : state_(std::make_shared<State>()) {}

AsyncOperationLifecycle::~AsyncOperationLifecycle()
{
    shutdown();
}

AsyncOperationLifecycle::Token AsyncOperationLifecycle::begin()
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->alive || state_->active)
        return {};
    state_->active = true;
    state_->request_id = 0;
    if (++state_->generation == 0)
        ++state_->generation;
    return Token(state_, state_->generation);
}

bool AsyncOperationLifecycle::activate(const Token &token, uint64_t request_id)
{
    const auto state = token.state_.lock();
    if (!state || state.get() != state_.get())
        return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->alive || !state->active || state->generation != token.generation_)
        return false;
    state->request_id = request_id;
    return true;
}

bool AsyncOperationLifecycle::abort(const Token &token)
{
    const auto state = token.state_.lock();
    if (!state || state.get() != state_.get())
        return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->active || state->generation != token.generation_)
        return false;
    state->active = false;
    state->request_id = 0;
    if (++state->generation == 0)
        ++state->generation;
    return true;
}

bool AsyncOperationLifecycle::active() const
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->alive && state_->active;
}

uint64_t AsyncOperationLifecycle::shutdown()
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    const uint64_t request_id = state_->request_id;
    state_->alive = false;
    state_->active = false;
    state_->request_id = 0;
    return request_id;
}

bool AsyncOperationLifecycle::Token::complete() const
{
    const auto state = state_.lock();
    if (!state)
        return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->alive || !state->active || state->generation != generation_)
        return false;
    state->active = false;
    state->request_id = 0;
    return true;
}

bool AsyncOperationLifecycle::Token::current() const
{
    const auto state = state_.lock();
    if (!state)
        return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->alive && state->generation == generation_;
}

} // namespace setting
