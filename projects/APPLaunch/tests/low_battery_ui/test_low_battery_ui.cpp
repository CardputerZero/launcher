/* SPDX-License-Identifier: MIT */
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../main/ui/ui_low_battery.cpp"
#include "../../main/ui/ui_screensaver.cpp"
#include "../../../../ext_components/cp0_lvgl/src/cp0_keyboard_queue.h"

extern "C" void test_keyboard_read(lv_indev_t *, lv_indev_data_t *);

static cp0_battery_info_t battery{};
static int shutdown_calls;
static int custom_events;
static int global_events;
static int previous_filter_calls;
static int battery_reads;
static std::string screensaver_timeout = "0";

decltype(cp0_signal_config_api) cp0_signal_config_api;
decltype(cp0_signal_audio_api) cp0_signal_audio_api;
decltype(cp0_signal_filesystem_api) cp0_signal_filesystem_api;

extern "C" cp0_battery_info_t cp0_battery_read() { ++battery_reads; return battery; }
extern "C" void cp0_system_shutdown() { ++shutdown_calls; }

static void set_battery(int soc, bool charging = false, bool valid = true)
{
    battery.soc = soc;
    battery.flags = charging ? 1 : 0;
    battery.valid = valid ? 1 : 0;
}

static void advance(uint32_t ms)
{
    lv_tick_inc(ms);
    launcher_battery_ui::timer_cb(launcher_battery_ui::warning_timer);
}

static bool visible()
{
    return launcher_battery_ui::overlay &&
        !lv_obj_has_flag(launcher_battery_ui::overlay, LV_OBJ_FLAG_HIDDEN);
}

static void key(uint32_t code, int state, bool consumed)
{
    key_item item{};
    item.key_code = code;
    item.key_state = state;
    const int custom_before = custom_events;
    const int global_before = global_events;
    assert(cp0_keyboard_queue_push(&item) == 0);
    lv_indev_data_t data{};
    test_keyboard_read(nullptr, &data);
    assert(custom_events == custom_before + (consumed ? 0 : 1));
    assert(global_events == global_before + (consumed ? 0 : 1));
    assert(consumed ? data.key == 0 : data.key != 0);
}

static int previous_filter(const key_item *)
{
    ++previous_filter_calls;
    return 0;
}

static void snapshot_if_requested(const char *variable = "BATTERY_UI_SNAPSHOT")
{
    const char *path = std::getenv(variable);
    if (!path) return;
    auto *buf = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_RGB888);
    assert(buf);
    FILE *file = std::fopen(path, "wb");
    assert(file);
    std::fprintf(file, "P6\n%u %u\n255\n", buf->header.w, buf->header.h);
    for (unsigned y = 0; y < buf->header.h; ++y) {
        const auto *row = buf->data + y * buf->header.stride;
        for (unsigned x = 0; x < buf->header.w; ++x) {
            const unsigned char rgb[] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
            std::fwrite(rgb, 1, 3, file);
        }
    }
    std::fclose(file);
    lv_draw_buf_destroy(buf);
}

static void wake_screensaver()
{
    assert(ui_screensaver_is_active());
    key(KEY_ENTER, KBD_KEY_PRESSED, true);
    assert(s_exiting && ui_screensaver_is_active() && !visible());
    advance(100);
    lv_anim_refr_now();
    assert(ui_screensaver_is_active() && !visible());
    key(KEY_ENTER, KBD_KEY_RELEASED, true);
    advance(400);
    lv_anim_refr_now();
    assert(!ui_screensaver_is_active());
    assert(lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN));
}

static void test_screensaver_battery_interaction()
{
    using namespace launcher_battery_ui;
    shutdown_warning();
    set_battery(30);
    init_warning();
    assert(!visible());
    start_screensaver();
    assert(ui_screensaver_is_active());

    // An unseen reminder is deferred until the working screen is fully visible.
    const int reads_before = battery_reads;
    set_battery(20);
    advance(3000);
    assert(battery_reads > reads_before);
    assert(flow.warning() == LowBatteryWarning::Reminder && !visible());
    update_warning(battery);
    assert(!visible());
    advance(60000);
    assert(!visible());
    assert(lv_obj_get_child(lv_layer_top(), -1) == s_overlay);
    snapshot_if_requested("BATTERY_SCREENSAVER_SNAPSHOT");
    wake_screensaver();
    assert(visible());
    snapshot_if_requested("BATTERY_WAKE_SNAPSHOT");
    key(KEY_ENTER, KBD_KEY_PRESSED, true);
    key(KEY_ENTER, KBD_KEY_RELEASED, true);
    assert(!visible());
    start_screensaver();
    advance(3000);
    wake_screensaver();
    assert(!visible());

    // Charging and unplugging still rearm the reminder under the screensaver.
    start_screensaver();
    set_battery(15, true);
    advance(3000);
    set_battery(15);
    advance(3000);
    assert(flow.warning() == LowBatteryWarning::Reminder && !visible());
    wake_screensaver();
    assert(visible());

    // Hidden warnings do not flash, steal the foreground, or pause a countdown.
    set_battery(4);
    advance(3000);
    assert(visible());
    start_screensaver();
    assert(!visible());
    const lv_opa_t hidden_opacity = lv_obj_get_style_bg_opa(tint, LV_PART_MAIN);
    advance(1000);
    assert(!visible() && lv_obj_get_style_bg_opa(tint, LV_PART_MAIN) == hidden_opacity);
    set_battery(3);
    advance(3000);
    assert(flow.warning() == LowBatteryWarning::ShutdownCountdown && !visible());
    advance(6000);
    wake_screensaver();
    assert(visible());
    assert(flow.seconds_until_shutdown(lv_tick_get()) == 9);
    assert(std::strcmp(lv_label_get_text(countdown_label), "Power off in 9s") == 0);
    start_screensaver();
    assert(!visible());
    const int shutdown_before = shutdown_calls;
    advance(8500);
    assert(shutdown_calls == shutdown_before + 1 && !visible());
    assert(lv_obj_get_child(lv_layer_top(), -1) == s_overlay);
    advance(3000);
    assert(shutdown_calls == shutdown_before + 1);

    // All cancellation paths continue to apply even with every warning hidden.
    for (int recovery : {0, 1, 2, 3}) {
        set_battery(30);
        advance(3000);
        set_battery(3);
        advance(3000);
        advance(12000);
        if (recovery == 0) set_battery(3, true);
        else if (recovery == 1) set_battery(3, false, false);
        else if (recovery == 2) set_battery(4);
        else set_battery(25);
        const int shutdown_before_recovery = shutdown_calls;
        advance(3000);
        assert(!visible() && shutdown_calls == shutdown_before_recovery);
        assert(!flow.shutdown_due(lv_tick_get()));
        wake_screensaver();
        assert(visible() == (recovery == 2));
        start_screensaver();
    }
    stop_screensaver();
    shutdown_warning();
}

int main()
{
    using namespace launcher_battery_ui;
    lv_init();
    auto *display = lv_display_create(320, 240);
    assert(display);
    LV_EVENT_KEYBOARD = lv_event_register_id();
    lv_obj_add_event_cb(lv_screen_active(), [](lv_event_t *) { ++custom_events; },
                       static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), nullptr);
    cp0_keyboard_set_global_key_handler([](const key_item *) { ++global_events; });
    cp0_keyboard_set_key_filter(previous_filter);
    cp0_signal_config_api.append([](std::list<std::string>, std::function<void(int, std::string)> reply) {
        if (reply) reply(0, screensaver_timeout);
    });
    cp0_signal_audio_api.append([](std::list<std::string>, std::function<void(int, std::string)> reply) {
        if (reply) reply(0, "");
    });
    cp0_signal_filesystem_api.append([](std::list<std::string>, std::function<void(int, std::string)> reply) {
        if (reply) reply(0, "/missing-screensaver-test-image.png");
    });
    ui_screensaver_init();
    assert(s_timer && s_overlay);

    set_battery(20);
    init_warning();
    assert(visible());
    assert(std::strstr(lv_label_get_text(message_label), "Press Enter to close"));
    snapshot_if_requested();
    advance(60000);
    assert(visible());
    set_battery(19);
    advance(3000);
    assert(std::strcmp(lv_label_get_text(countdown_label), "Battery: 19%") == 0);
    assert(lv_obj_get_style_bg_opa(tint, LV_PART_MAIN) == LV_OPA_TRANSP);
    key(KEY_ESC, KBD_KEY_PRESSED, true);
    key(KEY_ESC, KBD_KEY_RELEASED, true);
    assert(visible());
    key(KEY_ENTER, KBD_KEY_REPEATED, true);
    key(KEY_ENTER, KBD_KEY_RELEASED, true);
    assert(visible());

    screensaver_timeout = "10";
    advance(10000);
    ::timer_cb(s_timer);
    assert(ui_screensaver_is_active() && !visible());
    screensaver_timeout = "0";
    wake_screensaver();
    assert(visible());
    key(KEY_DOWN, KBD_KEY_PRESSED, true);
    key(KEY_ENTER, KBD_KEY_PRESSED, true);
    assert(!visible());
    key(KEY_ENTER, KBD_KEY_REPEATED, true);
    key(KEY_ENTER, KBD_KEY_RELEASED, true);
    key(KEY_DOWN, KBD_KEY_REPEATED, true);
    key(KEY_DOWN, KBD_KEY_RELEASED, true);
    key(KEY_ENTER, KBD_KEY_PRESSED, false);
    key(KEY_ENTER, KBD_KEY_RELEASED, false);
    assert(previous_filter_calls == 2);
    advance(60000);
    assert(!visible());

    set_battery(15, true);
    advance(3000);
    set_battery(15);
    advance(3000);
    assert(visible());
    key(KEY_KPENTER, KBD_KEY_PRESSED, true);
    assert(!visible());
    key(KEY_KPENTER, KBD_KEY_RELEASED, true);
    advance(3000);
    assert(!visible());

    set_battery(10, true);
    advance(3000);
    set_battery(10);
    advance(3000);
    key(KEY_ENTER, KBD_KEY_REPEATED, true);
    set_battery(10, false, false);
    advance(3000);
    assert(!visible());
    key(KEY_ENTER, KBD_KEY_RELEASED, true);
    set_battery(10);
    advance(3000);
    assert(!visible());

    set_battery(4);
    advance(3000);
    assert(visible());
    assert(lv_obj_get_style_bg_opa(tint, LV_PART_MAIN) == LV_OPA_20);
    advance(500);
    assert(lv_obj_get_style_bg_opa(tint, LV_PART_MAIN) == LV_OPA_50);
    key(KEY_ENTER, KBD_KEY_PRESSED, false);
    key(KEY_ENTER, KBD_KEY_RELEASED, false);
    assert(visible());

    set_battery(3);
    advance(3000);
    assert(std::strcmp(lv_label_get_text(countdown_label), "Power off in 15s") == 0);
    advance(12000);
    set_battery(3, true);
    advance(3000);
    assert(shutdown_calls == 0 && !visible());
    set_battery(3);
    advance(3000);
    advance(15000);
    assert(shutdown_calls == 1);
    advance(3000);
    assert(shutdown_calls == 1);

    set_battery(10);
    advance(3000);
    lv_obj_delete(message_label);
    advance(250);
    assert(message_label && visible());
    shutdown_warning();
    assert(!overlay && !warning_timer);
    assert(cp0_keyboard_get_key_filter() == previous_filter);
    test_screensaver_battery_interaction();
    ui_screensaver_deinit();
    cp0_keyboard_set_key_filter(nullptr);
    lv_display_delete(display);
    lv_deinit();
    std::puts("Battery UI and keyboard dispatch checks passed");
}
