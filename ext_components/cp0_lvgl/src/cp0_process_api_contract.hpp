#pragma once

#include <charconv>
#include <cerrno>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <new>
#include <list>
#include <string>
#include <string_view>
#include <vector>

namespace cp0_process_api_contract {

template <typename Operation>
inline int invoke_c_api(Operation &&operation) noexcept
{
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc &) {
        return -ENOMEM;
    } catch (...) {
        return -EIO;
    }
}

template <typename Operation>
inline void invoke_c_api_void(Operation &&operation) noexcept
{
    try {
        std::forward<Operation>(operation)();
    } catch (...) {
    }
}

inline constexpr int MAX_GRACE_MS = 300000;
inline constexpr int MAX_DELAY_MS = 300000;

inline bool has_exact_arguments(const std::list<std::string> &args, std::size_t count)
{
    return args.size() == count;
}

inline bool has_at_least_arguments(const std::list<std::string> &args, std::size_t count)
{
    return args.size() >= count;
}

inline const std::string *argument_at(const std::list<std::string> &args, std::size_t index)
{
    auto iterator = args.begin();
    while (index > 0 && iterator != args.end()) {
        --index;
        ++iterator;
    }
    return iterator == args.end() ? nullptr : &*iterator;
}

inline std::vector<std::string> arguments_from(const std::list<std::string> &args,
                                               std::size_t index)
{
    std::vector<std::string> result;
    auto iterator = args.begin();
    while (index > 0 && iterator != args.end()) {
        --index;
        ++iterator;
    }
    result.reserve(static_cast<std::size_t>(std::distance(iterator, args.end())));
    for (; iterator != args.end(); ++iterator) result.push_back(*iterator);
    return result;
}

template <typename Integer>
inline bool parse_integer(std::string_view text, Integer minimum, Integer maximum, Integer &value)
{
    if (text.empty()) return false;
    Integer parsed = 0;
    const char *begin = text.data();
    const char *end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed < minimum || parsed > maximum)
        return false;
    value = parsed;
    return true;
}

inline bool parse_bool(std::string_view text, bool &value)
{
    int parsed = 0;
    if (!parse_integer(text, 0, 1, parsed)) return false;
    value = parsed != 0;
    return true;
}

inline bool parse_pid(std::string_view text, int &value)
{
    return parse_integer(text, 1, std::numeric_limits<int>::max(), value);
}

inline bool parse_grace_ms(std::string_view text, int &value)
{
    return parse_integer(text, 0, MAX_GRACE_MS, value);
}

inline bool parse_delay_ms(std::string_view text, int &value)
{
    return parse_integer(text, 0, MAX_DELAY_MS, value);
}

inline bool parse_pointer(std::string_view text, uintptr_t &value)
{
    return parse_integer(text, uintptr_t{0}, std::numeric_limits<uintptr_t>::max(), value);
}

inline bool parse_spawn_response(std::string_view text, int &pid)
{
    return parse_pid(text, pid);
}

inline bool parse_lock_holder_response(std::string_view text, int &pid)
{
    return parse_integer(text, 0, std::numeric_limits<int>::max(), pid);
}

inline void invoke_callback_safely(
    const std::function<void(int, std::string)> &callback,
    int code,
    const std::string &data) noexcept
{
    if (!callback) return;
    try {
        callback(code, data);
    } catch (...) {
    }
}

inline std::list<std::string> make_run_sudo_request(const char *password,
                                                    const char *const *argv)
{
    std::list<std::string> request = {"RunSudo", password ? password : ""};
    if (argv) {
        for (int index = 0; argv[index]; ++index)
            request.emplace_back(argv[index]);
    }
    return request;
}

} // namespace cp0_process_api_contract
