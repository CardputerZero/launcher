#include "../main/ui/model/setup_wifi_model.hpp"

#include <cassert>
#include <string>

int main()
{
    int timer = 0;
    int stale_timer = 0;
    static_assert(noexcept(setup_wifi_feedback_timer_callback_ready(
        static_cast<int *>(nullptr), static_cast<int *>(nullptr),
        SetupWifiFeedbackModel::Token{})));
    assert(setup_wifi_feedback_timer_callback_ready(
        &timer, &timer, SetupWifiFeedbackModel::Token{1}));
    assert(!setup_wifi_feedback_timer_callback_ready(
        &stale_timer, &timer, SetupWifiFeedbackModel::Token{1}));
    assert(!setup_wifi_feedback_timer_callback_ready(
        static_cast<int *>(nullptr), &timer, SetupWifiFeedbackModel::Token{1}));
    assert(!setup_wifi_feedback_timer_callback_ready(
        &timer, &timer, SetupWifiFeedbackModel::Token{}));

    int feedback_screen = 0;
    int stale_screen = 0;
    static_assert(noexcept(setup_wifi_feedback_screen_delete_allowed(
        &feedback_screen, &feedback_screen, &feedback_screen)));
    assert(setup_wifi_feedback_screen_delete_allowed(
        &feedback_screen, &feedback_screen, &feedback_screen));
    assert(!setup_wifi_feedback_screen_delete_allowed(
        &feedback_screen, &stale_screen, &feedback_screen));
    assert(!setup_wifi_feedback_screen_delete_allowed(
        &stale_screen, &stale_screen, &feedback_screen));
    assert(!setup_wifi_feedback_screen_delete_allowed(
        static_cast<int *>(nullptr), &feedback_screen, &feedback_screen));

    int input_label = 0;
    int stale_label = 0;
    static_assert(noexcept(setup_wifi_owned_label_delete_matches(
        &input_label, &input_label, &input_label)));
    assert(setup_wifi_owned_label_delete_matches(
        &input_label, &input_label, &input_label));
    assert(!setup_wifi_owned_label_delete_matches(
        &input_label, &stale_label, &input_label));
    assert(!setup_wifi_owned_label_delete_matches(
        &stale_label, &stale_label, &input_label));
    assert(!setup_wifi_owned_label_delete_matches(
        static_cast<int *>(nullptr), &input_label, &input_label));

    SetupWifiPasswordModel model;
    assert(!model.can_submit());
    assert(model.masked_display() == "_");

    model.begin("Office WiFi");
    assert(!model.can_submit());
    assert(model.append("secret"));
    assert(model.can_submit());
    assert(model.password() == "secret");
    assert(model.masked_display() == "******_");
    assert(model.masked_display().find("secret") == std::string::npos);
    assert(!model.append("\n"));
    assert(!model.append("\x80"));
    assert(!model.append("\xc3"));
    assert(!model.append("\xc0\x80"));
    assert(!model.append("\xed\xa0\x80"));

    assert(model.append("\xc3\xa9"));
    assert(model.masked_display() == "*******_");
    assert(model.erase_last());
    assert(model.password() == "secret");

    model.clear_password();
    assert(model.append(std::string(62, 'x')));
    assert(!model.append("\xe2\x82\xac"));
    assert(model.password().size() == 62);
    model.clear_password();
    assert(model.append(std::string(SetupWifiPasswordModel::MAX_PASSWORD_BYTES, 'x')));
    assert(!model.append("y"));
    model.reset();
    assert(model.ssid().empty());
    assert(model.password().empty());
    assert(!model.can_submit());

    SetupWifiFeedbackModel feedback;
    assert(!feedback.pending());
    const auto first_feedback = feedback.begin();
    assert(first_feedback);
    assert(feedback.pending());
    assert(!feedback.begin());
    assert(feedback.complete(first_feedback));
    assert(!feedback.pending());
    assert(!feedback.complete(first_feedback));
    const auto second_feedback = feedback.begin();
    assert(second_feedback && second_feedback != first_feedback);
    assert(!feedback.complete(first_feedback));
    assert(feedback.pending());
    assert(feedback.cancel(second_feedback));
    assert(!feedback.pending());
}
