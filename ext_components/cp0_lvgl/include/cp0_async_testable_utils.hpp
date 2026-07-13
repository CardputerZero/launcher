#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_set>

namespace cp0_testable {

constexpr bool is_leap_year(int year)
{
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

constexpr int days_in_month(int year, int month)
{
    constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month >= 1 && month <= 12
        ? (month == 2 && is_leap_year(year) ? 29 : days[month - 1])
        : 0;
}

constexpr int clamp_day(int year, int month, int day)
{
    const int maximum = days_in_month(year, month);
    return maximum == 0 ? 0 : std::max(1, std::min(day, maximum));
}

inline void append_tail(std::string &tail, const char *data, std::size_t size,
                        std::size_t capacity)
{
    if (capacity == 0) {
        tail.clear();
        return;
    }
    if (size >= capacity) {
        tail.assign(data + size - capacity, capacity);
        return;
    }
    if (tail.size() + size > capacity)
        tail.erase(0, tail.size() + size - capacity);
    tail.append(data, size);
}

constexpr bool pending_output_fits(std::size_t pending, std::size_t incoming,
                                   std::size_t capacity)
{
    return pending <= capacity && incoming <= capacity - pending;
}

constexpr int prefer_io_error(int io_error, int process_exit_code)
{
    return io_error ? io_error : process_exit_code;
}

class RecentIdHistory {
public:
    explicit RecentIdHistory(std::size_t capacity) : capacity_(capacity) {}

    void remember(std::uint64_t id)
    {
        if (capacity_ == 0 || !ids_.insert(id).second) return;
        order_.push_back(id);
        if (order_.size() > capacity_) {
            ids_.erase(order_.front());
            order_.pop_front();
        }
    }

    bool contains(std::uint64_t id) const { return ids_.count(id) != 0; }
    std::size_t size() const { return ids_.size(); }

private:
    std::size_t capacity_;
    std::deque<std::uint64_t> order_;
    std::unordered_set<std::uint64_t> ids_;
};

constexpr bool callback_event_fits_tick(std::size_t processed_events,
                                        std::size_t processed_output_bytes,
                                        std::size_t next_output_bytes,
                                        std::size_t event_limit,
                                        std::size_t output_byte_limit)
{
    if (processed_events >= event_limit) return false;
    // Always allow one event so a configuration smaller than a producer chunk
    // cannot permanently block the FIFO head.
    return processed_events == 0 || next_output_bytes == 0 ||
           pending_output_fits(processed_output_bytes, next_output_bytes,
                               output_byte_limit);
}

enum class AuthCompletionAction { CANCEL, RETRY, FINISH };

constexpr AuthCompletionAction auth_completion_action(bool cancel_requested,
                                                       bool auth_failed,
                                                       int attempts,
                                                       int max_attempts)
{
    if (cancel_requested) return AuthCompletionAction::CANCEL;
    if (auth_failed && attempts < max_attempts)
        return AuthCompletionAction::RETRY;
    return AuthCompletionAction::FINISH;
}

template <typename TimePoint>
constexpr bool prompt_deadline_expired(int timeout_ms, const TimePoint &now,
                                       const TimePoint &deadline)
{
    return timeout_ms > 0 && now >= deadline;
}

} // namespace cp0_testable
