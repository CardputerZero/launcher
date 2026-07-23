#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

template <typename CallbackTimer, typename ActiveTimer, typename Token>
constexpr bool setup_wifi_feedback_timer_callback_ready(
    CallbackTimer callback_timer, ActiveTimer active_timer, Token token) noexcept
{
    return callback_timer && callback_timer == active_timer && token != Token{};
}

template <typename Target, typename CurrentTarget, typename TrackedScreen>
constexpr bool setup_wifi_feedback_screen_delete_allowed(
    Target target, CurrentTarget current_target, TrackedScreen tracked_screen) noexcept
{
    return target && target == current_target && target == tracked_screen;
}

template <typename EventTarget, typename CurrentTarget, typename OwnedLabel>
constexpr bool setup_wifi_owned_label_delete_matches(EventTarget event_target,
                                                      CurrentTarget current_target,
                                                      OwnedLabel owned_label) noexcept
{
    return event_target && event_target == current_target &&
           event_target == owned_label;
}

class SetupWifiPasswordModel
{
public:
    // WPA passphrases use up to 63 bytes; a raw hexadecimal PSK uses 64.
    static constexpr std::size_t MAX_PASSWORD_BYTES = 64;

    ~SetupWifiPasswordModel();

    void begin(std::string ssid);
    void reset();
    bool append(const std::string &text);
    bool erase_last();
    void clear_password();

    bool can_submit() const { return !ssid_.empty() && !password_.empty(); }
    const std::string &ssid() const { return ssid_; }
    const std::string &password() const { return password_; }
    std::string masked_display() const;

private:
    void secure_clear_password();
    std::string ssid_;
    std::string password_;
};

class SetupWifiFeedbackModel
{
public:
    using Token = std::uint64_t;

    Token begin();
    bool complete(Token token);
    bool cancel(Token token);
    bool pending() const { return pending_; }

private:
    bool pending_ = false;
    Token generation_ = 0;
};
