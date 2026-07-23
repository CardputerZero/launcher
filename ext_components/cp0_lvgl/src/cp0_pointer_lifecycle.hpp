#pragma once

#include <type_traits>
#include <utility>

namespace cp0 {

template <typename T>
bool can_mount_child(T *parent, T *current)
{
    return parent != nullptr && current == nullptr;
}

template <typename T>
bool display_references_screen(T *owned, T *active, T *loading, T *previous)
{
    return owned != nullptr &&
           (owned == active || owned == loading || owned == previous);
}

template <typename T>
bool clear_if_deleted(T *&current, T *deleted)
{
    if (!deleted || current != deleted) return false;
    current = nullptr;
    return true;
}

template <typename T>
bool is_direct_event_target(T *target, T *current_target)
{
    return target != nullptr && target == current_target;
}

constexpr int app_content_height(bool has_bottom_bar)
{
    return has_bottom_bar ? 130 : 150;
}

template <typename Type>
bool input_type_uses_group(Type type, Type keypad, Type encoder)
{
    return type == keypad || type == encoder;
}

template <typename Callback>
class RollbackGuard
{
public:
    explicit RollbackGuard(Callback callback)
        : callback_(std::move(callback))
    {
    }

    ~RollbackGuard() noexcept
    {
        if (!active_) return;
        try {
            callback_();
        } catch (...) {
        }
    }

    RollbackGuard(const RollbackGuard &) = delete;
    RollbackGuard &operator=(const RollbackGuard &) = delete;
    RollbackGuard(RollbackGuard &&) = delete;
    RollbackGuard &operator=(RollbackGuard &&) = delete;

    void dismiss() noexcept { active_ = false; }

private:
    Callback callback_;
    bool active_ = true;
};

template <typename Callback>
RollbackGuard<std::decay_t<Callback>> make_rollback_guard(Callback &&callback)
{
    return RollbackGuard<std::decay_t<Callback>>(
        std::forward<Callback>(callback));
}

} // namespace cp0
