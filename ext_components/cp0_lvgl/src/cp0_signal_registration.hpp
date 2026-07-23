#pragma once

#include <mutex>
#include <memory>
#include <tuple>
#include <utility>

namespace cp0 {

template <typename CallbackList>
class SignalRegistration {
public:
    using Callback = typename CallbackList::Callback;
    using Handle = typename CallbackList::Handle;

    SignalRegistration() = default;
    ~SignalRegistration() { reset(); }

    SignalRegistration(const SignalRegistration &) = delete;
    SignalRegistration &operator=(const SignalRegistration &) = delete;

    SignalRegistration(SignalRegistration &&other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        list_ = other.list_;
        handle_ = std::move(other.handle_);
        other.list_ = nullptr;
        other.handle_ = Handle{};
    }

    SignalRegistration &operator=(SignalRegistration &&other) noexcept
    {
        if (this == &other)
            return *this;

        CallbackList *old_list = nullptr;
        Handle old_handle;
        {
            std::scoped_lock lock(mutex_, other.mutex_);
            old_list = list_;
            old_handle = std::move(handle_);
            list_ = other.list_;
            handle_ = std::move(other.handle_);
            other.list_ = nullptr;
            other.handle_ = Handle{};
        }
        if (old_list && old_handle)
            old_list->remove(old_handle);
        return *this;
    }

    bool replace(CallbackList &list, const Callback &callback)
    {
        Handle replacement = list.append(callback);
        if (!replacement)
            return false;

        CallbackList *old_list = nullptr;
        Handle old_handle;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            old_list = list_;
            old_handle = std::move(handle_);
            list_ = &list;
            handle_ = std::move(replacement);
        }
        if (old_list && old_handle)
            old_list->remove(old_handle);
        return true;
    }

    void reset()
    {
        CallbackList *list = nullptr;
        Handle handle;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            list = list_;
            handle = std::move(handle_);
            list_ = nullptr;
            handle_ = Handle{};
        }
        if (list && handle)
            list->remove(handle);
    }

    bool registered() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return list_ != nullptr && static_cast<bool>(handle_);
    }

private:
    mutable std::mutex mutex_;
    CallbackList *list_ = nullptr;
    Handle handle_;
};

template <typename CallbackList>
struct SignalBinding {
    CallbackList *list;
    typename CallbackList::Callback callback;
};

template <typename CallbackList, typename Callback>
SignalBinding<CallbackList> bind_signal(CallbackList &list, Callback &&callback)
{
    return {&list, typename CallbackList::Callback(std::forward<Callback>(callback))};
}

template <typename... CallbackLists>
class SignalRegistrationBundle {
public:
    SignalRegistrationBundle() = default;
    ~SignalRegistrationBundle() { reset(); }

    SignalRegistrationBundle(const SignalRegistrationBundle &) = delete;
    SignalRegistrationBundle &operator=(const SignalRegistrationBundle &) = delete;
    SignalRegistrationBundle(SignalRegistrationBundle &&) = delete;
    SignalRegistrationBundle &operator=(SignalRegistrationBundle &&) = delete;

    bool replace(SignalBinding<CallbackLists>... bindings)
    {
        const auto binding_tuple = std::make_tuple(std::move(bindings)...);
        if (!bindings_valid(binding_tuple,
                            std::index_sequence_for<CallbackLists...>{}))
            return false;

        auto replacement = std::make_unique<State>();
        if (!register_all(*replacement, binding_tuple,
                          std::index_sequence_for<CallbackLists...>{}))
            return false;

        std::unique_ptr<State> previous;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            previous = std::move(state_);
            state_ = std::move(replacement);
        }
        return true;
    }

    void reset()
    {
        std::unique_ptr<State> previous;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            previous = std::move(state_);
        }
    }

    bool registered() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ != nullptr;
    }

private:
    struct State {
        std::tuple<SignalRegistration<CallbackLists>...> registrations;
    };

    template <typename Bindings, std::size_t... Indexes>
    static bool bindings_valid(const Bindings &bindings,
                               std::index_sequence<Indexes...>)
    {
        return ((std::get<Indexes>(bindings).list != nullptr) && ...);
    }

    template <typename Bindings, std::size_t... Indexes>
    static bool register_all(State &state, const Bindings &bindings,
                             std::index_sequence<Indexes...>)
    {
        return (std::get<Indexes>(state.registrations)
                    .replace(*std::get<Indexes>(bindings).list,
                             std::get<Indexes>(bindings).callback) && ...);
    }

    mutable std::mutex mutex_;
    std::unique_ptr<State> state_;
};

} // namespace cp0
