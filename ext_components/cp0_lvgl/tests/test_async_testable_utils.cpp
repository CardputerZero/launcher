#include "cp0_async_testable_utils.hpp"

#include <cassert>
#include <string>

int main()
{
    using namespace cp0_testable;

    static_assert(is_leap_year(2000));
    static_assert(!is_leap_year(1900));
    static_assert(is_leap_year(2024));
    static_assert(!is_leap_year(2023));
    static_assert(days_in_month(2024, 2) == 29);
    static_assert(days_in_month(2023, 2) == 28);
    static_assert(days_in_month(2026, 4) == 30);
    static_assert(days_in_month(2026, 1) == 31);
    static_assert(days_in_month(2026, 0) == 0);
    static_assert(days_in_month(2026, 13) == 0);
    static_assert(clamp_day(2023, 2, 31) == 28);
    static_assert(clamp_day(2024, 2, 31) == 29);
    static_assert(clamp_day(2024, 2, 0) == 1);

    std::string tail;
    append_tail(tail, "abc", 3, 5);
    assert(tail == "abc");
    append_tail(tail, "def", 3, 5);
    assert(tail == "bcdef");
    append_tail(tail, "0123456789", 10, 5);
    assert(tail == "56789");
    append_tail(tail, "ignored", 7, 0);
    assert(tail.empty());

    assert(pending_output_fits(0, 256, 256));
    assert(pending_output_fits(255, 1, 256));
    assert(!pending_output_fits(255, 2, 256));
    assert(!pending_output_fits(257, 0, 256));
    static_assert(prefer_io_error(-5, 0) == -5);
    static_assert(prefer_io_error(0, 7) == 7);

    RecentIdHistory terminal_ids(3);
    terminal_ids.remember(10);
    terminal_ids.remember(11);
    terminal_ids.remember(10);
    assert(terminal_ids.size() == 2);
    assert(terminal_ids.contains(10));
    assert(terminal_ids.contains(11));
    terminal_ids.remember(12);
    terminal_ids.remember(13);
    assert(terminal_ids.size() == 3);
    assert(!terminal_ids.contains(10));
    assert(terminal_ids.contains(11));
    assert(terminal_ids.contains(12));
    assert(terminal_ids.contains(13));
    RecentIdHistory disabled_history(0);
    disabled_history.remember(1);
    assert(!disabled_history.contains(1));

    constexpr std::size_t event_limit = 64;
    constexpr std::size_t byte_limit = 64 * 1024;
    static_assert(callback_event_fits_tick(0, 0, byte_limit + 1,
                                           event_limit, byte_limit));
    static_assert(callback_event_fits_tick(63, byte_limit - 1, 1,
                                           event_limit, byte_limit));
    static_assert(!callback_event_fits_tick(63, byte_limit, 1,
                                            event_limit, byte_limit));
    static_assert(callback_event_fits_tick(63, byte_limit, 0,
                                           event_limit, byte_limit));
    static_assert(!callback_event_fits_tick(64, 0, 0,
                                            event_limit, byte_limit));

    static_assert(auth_completion_action(false, true, 1, 3) ==
                  AuthCompletionAction::RETRY);
    static_assert(auth_completion_action(false, true, 2, 3) ==
                  AuthCompletionAction::RETRY);
    static_assert(auth_completion_action(false, true, 3, 3) ==
                  AuthCompletionAction::FINISH);
    static_assert(auth_completion_action(false, false, 1, 3) ==
                  AuthCompletionAction::FINISH);
    static_assert(auth_completion_action(true, true, 1, 3) ==
                  AuthCompletionAction::CANCEL);
    static_assert(auth_completion_action(true, false, 3, 3) ==
                  AuthCompletionAction::CANCEL);

    static_assert(!prompt_deadline_expired(0, 100, 100));
    static_assert(!prompt_deadline_expired(-1, 101, 100));
    static_assert(!prompt_deadline_expired(1000, 99, 100));
    static_assert(prompt_deadline_expired(1000, 100, 100));
    static_assert(prompt_deadline_expired(1000, 101, 100));
}
