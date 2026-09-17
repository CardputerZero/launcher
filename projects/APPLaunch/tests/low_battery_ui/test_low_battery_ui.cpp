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
static int backlight_maximum = 255;
static int backlight_raw = 128;
static int backlight_suspends;
static int backlight_restores;
static bool fail_backlight_write;
static std::string screensaver_timeout = "0";

decltype(cp0_signal_config_api) cp0_signal_config_api;
decltype(cp0_signal_audio_api) cp0_signal_audio_api;
decltype(cp0_signal_filesystem_api) cp0_signal_filesystem_api;
decltype(cp0_signal_settings_api) cp0_signal_settings_api;
decltype(cp0_signal_system_play) cp0_signal_system_play;

/* The sound worker runs off the LVGL thread; the last requested asset is
 * recorded synchronously so expectations do not race it. */
static void assert_sound(const char *expected)
{
    assert(std::strcmp(s_last_sound_asset, expected) == 0);
}

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

/* Unlock the way a user does: any key brings the visible screen up, TAB arms the
 * unlock, ENTER confirms it and the panel slides away. */
static void unlock_screensaver()
{
    assert(ui_screensaver_is_active());
    assert(s_lock.state() == LockscreenState::Locked);

    key(KEY_ESC, KBD_KEY_PRESSED, true);            // (1) -> (2)
    assert(s_lock.state() == LockscreenState::PendingUnlock);
    key(KEY_ESC, KBD_KEY_RELEASED, true);
    key(KEY_TAB, KBD_KEY_PRESSED, true);            // (2) -> (3)
    assert(s_lock.state() == LockscreenState::Armed);
    key(KEY_TAB, KBD_KEY_RELEASED, true);

    key(KEY_ENTER, KBD_KEY_PRESSED, true);          // (3) -> unlock
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

/* (2) -> (3) -> unlock, for callers that already reached (2). */
static void unlock_screensaver_from_pending_unlock()
{
    assert(s_lock.state() == LockscreenState::PendingUnlock);
    key(KEY_TAB, KBD_KEY_PRESSED, true);
    assert(s_lock.state() == LockscreenState::Armed);
    key(KEY_TAB, KBD_KEY_RELEASED, true);
    key(KEY_ENTER, KBD_KEY_PRESSED, true);
    assert(s_exiting && ui_screensaver_is_active());
    key(KEY_ENTER, KBD_KEY_RELEASED, true);
    advance(400);
    lv_anim_refr_now();
    assert(!ui_screensaver_is_active());
}

static void test_hold_gesture_enters_lock()
{
    /* A healthy pack keeps the low-battery reminder quiet: while a reminder is
     * up its own filter owns every key, so it would swallow the gesture. */
    set_battery(80);
    advance(3000);
    assert(!visible());

    // The idle timeout stays off so only the gesture can lock the panel.
    screensaver_timeout = "0";
    assert(!ui_screensaver_is_active());
    backlight_raw = 128;
    backlight_suspends = 0;
    backlight_restores = 0;

    // Until the threshold matures the key still belongs to the page.
    key(KEY_TAB, KBD_KEY_PRESSED, false);
    advance(2999);
    ::timer_cb(s_timer);
    assert(!ui_screensaver_is_active());
    assert(backlight_raw == 128 && backlight_suspends == 0);

    // Auto-repeat keeps the original window instead of restarting it.
    key(KEY_TAB, KBD_KEY_REPEATED, false);
    advance(1);
    ::timer_cb(s_timer);
    assert(ui_screensaver_is_active());
    assert(s_lock.state() == LockscreenState::Locked);
    assert(s_panel.black);
    assert(backlight_raw == 0 && backlight_suspends == 1);
    assert(!s_model.hold_pending());
    assert(s_block && lv_obj_has_flag(s_block, LV_OBJ_FLAG_HIDDEN));
    assert(s_hint && lv_obj_has_flag(s_hint, LV_OBJ_FLAG_HIDDEN));
    assert_sound("lock.mp3");

    /* The held key cannot walk the machine: its repeats and its release are not
     * fresh presses, so letting go leaves the panel black. */
    key(KEY_TAB, KBD_KEY_REPEATED, true);
    key(KEY_TAB, KBD_KEY_RELEASED, true);
    assert(s_lock.state() == LockscreenState::Locked);
    assert(s_panel.black && backlight_raw == 0);
    assert_sound("lock.mp3");

    // The next fresh press brings the visible screen up.
    key(KEY_ESC, KBD_KEY_PRESSED, true);
    assert(s_lock.state() == LockscreenState::PendingUnlock);
    assert_sound("select.mp3");
    key(KEY_ESC, KBD_KEY_RELEASED, true);
    unlock_screensaver_from_pending_unlock();
}

static void test_screensaver_panel()
{
    // Keep the low-battery reminder out of the way.
    set_battery(80);
    advance(3000);
    assert(!visible());

    lv_display_t *display = lv_display_get_default();
    assert(display);
    const int width = lv_display_get_horizontal_resolution(display);
    const int height = lv_display_get_vertical_resolution(display);
    const int top = AppPageRoot::kTopBarHeightPx;
    assert(top > 0 && height > top);

    backlight_raw = 128;
    backlight_suspends = 0;
    backlight_restores = 0;
    screensaver_timeout = "10";
    advance(10000);
    ::timer_cb(s_timer);

    /* (1) is pure black over the whole display, so it reads as a black screen on
     * every backend; the backlight going down is only a power optimisation. */
    assert(ui_screensaver_is_active());
    assert(!visible());
    assert(s_lock.state() == LockscreenState::Locked);
    assert(s_panel.black);
    assert(s_panel.x == 0 && s_panel.y == 0);
    assert(s_panel.width == width && s_panel.height == height);
    assert(lv_color_eq(lv_obj_get_style_bg_color(s_overlay, LV_PART_MAIN), lv_color_black()));
    assert(backlight_raw == 0 && backlight_suspends == 1);
    assert(s_screen_off_backlight_raw == 128);
    // No artwork is painted, and no frames are driven for a static panel.
    assert(s_block && lv_obj_has_flag(s_block, LV_OBJ_FLAG_HIDDEN));
    assert(s_hint && lv_obj_has_flag(s_hint, LV_OBJ_FLAG_HIDDEN));
    assert_sound("lock.mp3");

    /* (2) shows the wallpaper with the page's top bar in place and
     * the backlight back, and asks for the first unlock step. */
    screensaver_timeout = "0";
    key(KEY_ESC, KBD_KEY_PRESSED, true);
    assert(s_lock.state() == LockscreenState::PendingUnlock);
    assert(backlight_raw == 128 && backlight_restores == 1);
    assert(!s_panel.black);
    assert(s_panel.y == top && s_panel.height == height - top);
    assert(lv_obj_get_style_bg_image_src(s_overlay, LV_PART_MAIN) == s_background_cache.image());
    assert(s_hint && !lv_obj_has_flag(s_hint, LV_OBJ_FLAG_HIDDEN));
    assert(std::strcmp(lv_label_get_text(s_hint), "TAB & ENTER to unlock") == 0);
    lv_obj_update_layout(s_overlay);
    assert(lv_obj_get_y(s_hint) == 0);
    assert(lv_obj_get_height(s_hint) == top);
    assert(lv_obj_get_width(s_hint) == width - 116);
    lv_point_t text_size{};
    lv_text_get_size(&text_size, lv_label_get_text(s_hint), &lv_font_montserrat_14,
                     0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(text_size.x <= lv_obj_get_width(s_hint) - 5);
    assert(lv_obj_get_style_bg_opa(s_hint, LV_PART_MAIN) == LV_OPA_COVER);
    assert(lv_color_eq(lv_obj_get_style_bg_color(s_hint, LV_PART_MAIN), lv_color_black()));
    auto *snapshot = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_RGB888);
    assert(snapshot);
    int yellow_pixels = 0;
    for (int y = 0; y < top; ++y) {
        for (int x = 0; x < width - 116; ++x) {
            const auto *pixel = static_cast<const uint8_t *>(lv_draw_buf_goto_xy(snapshot, x, y));
            if (pixel[2] > 100 && pixel[1] > 70 && pixel[0] < 30) ++yellow_pixels;
        }
    }
    lv_draw_buf_destroy(snapshot);
    assert(yellow_pixels > 100);
    assert_sound("select.mp3");
    key(KEY_ESC, KBD_KEY_RELEASED, true);

    // (3) switches the hint to the confirmation step.
    key(KEY_TAB, KBD_KEY_PRESSED, true);
    assert(s_lock.state() == LockscreenState::Armed);
    assert(std::strcmp(lv_label_get_text(s_hint), "Press ENTER to unlock") == 0);
    lv_text_get_size(&text_size, lv_label_get_text(s_hint), &lv_font_montserrat_14,
                     0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(text_size.x <= lv_obj_get_width(s_hint) - 5);
    assert_sound("select.mp3");
    key(KEY_TAB, KBD_KEY_RELEASED, true);

    // A key that is not the confirmation steps back to (2).
    key(KEY_ESC, KBD_KEY_PRESSED, true);
    assert(s_lock.state() == LockscreenState::PendingUnlock);
    assert(std::strcmp(lv_label_get_text(s_hint), "TAB & ENTER to unlock") == 0);
    assert_sound("blocked.mp3");
    key(KEY_ESC, KBD_KEY_RELEASED, true);

    /* The 10 s timeout drops the visible state back to the black screen and puts
     * the backlight down again. */
    const uint32_t before_sleep = lv_tick_get();
    advance(9999);
    ::timer_cb(s_timer);
    assert(s_lock.state() == LockscreenState::PendingUnlock);
    advance(1);
    ::timer_cb(s_timer);
    assert(s_lock.state() == LockscreenState::Locked);
    assert(s_panel.black);
    assert(lv_obj_get_style_bg_image_src(s_overlay, LV_PART_MAIN) == nullptr);
    assert(backlight_raw == 0);
    assert(lv_tick_get() > before_sleep);
    assert(s_hint && lv_obj_has_flag(s_hint, LV_OBJ_FLAG_HIDDEN));
    assert_sound("lock.mp3");

    // ...and the countdown starts over from the next press.
    key(KEY_ESC, KBD_KEY_PRESSED, true);
    assert(s_lock.state() == LockscreenState::PendingUnlock);
    key(KEY_ESC, KBD_KEY_RELEASED, true);
    advance(5000);
    ::timer_cb(s_timer);
    key(KEY_ESC, KBD_KEY_PRESSED, true);
    key(KEY_ESC, KBD_KEY_RELEASED, true);
    advance(9999);
    ::timer_cb(s_timer);
    assert(s_lock.state() == LockscreenState::PendingUnlock);
    advance(1);
    ::timer_cb(s_timer);
    assert(s_lock.state() == LockscreenState::Locked);

    // Full unlock slides the wallpaper away and hides the hint.
    key(KEY_ESC, KBD_KEY_PRESSED, true);
    key(KEY_ESC, KBD_KEY_RELEASED, true);
    key(KEY_TAB, KBD_KEY_PRESSED, true);
    key(KEY_TAB, KBD_KEY_RELEASED, true);
    key(KEY_ENTER, KBD_KEY_PRESSED, true);
    assert_sound("unlock.mp3");
    assert(s_exiting && !s_panel.black);
    assert(s_hint && lv_obj_has_flag(s_hint, LV_OBJ_FLAG_HIDDEN));
    key(KEY_ENTER, KBD_KEY_RELEASED, true);
    advance(400);
    lv_anim_refr_now();
    assert(!ui_screensaver_is_active());
    assert(lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN));
}

static void test_background_cache()
{
    LockscreenBackgroundCache cache;
    assert(cache.load(TEST_LOCKSCREEN_BACKGROUND));
    assert(cache.image()->header.w == 320 && cache.image()->header.h == 150);
    const uint8_t *pixels = cache.image()->data;
    // Further activations reuse the owned decoded pixels, without reopening a file.
    assert(cache.load("/missing-lockscreen.png"));
    assert(cache.image()->data == pixels);
    cache.reset();
    assert(!cache.image());
    assert(!cache.load("/missing-lockscreen.png"));
    assert(!cache.load(TEST_LOCKSCREEN_BACKGROUND));
    cache.reset();
    assert(cache.load(TEST_LOCKSCREEN_BACKGROUND));
}

static void test_unlock_restarts_idle_timeout()
{
    set_battery(80);
    advance(3000);
    assert(!visible());
    screensaver_timeout = "10";
    backlight_raw = 128;

    for (bool poll_during_exit : {false, true}) {
        key(KEY_ESC, KBD_KEY_PRESSED, false);
        key(KEY_ESC, KBD_KEY_RELEASED, false);
        advance(10000);
        ::timer_cb(s_timer);
        assert(ui_screensaver_is_active() && s_panel.black);

        // Keep auto-lock enabled after a long stay on the lock screen.
        advance(30000);
        key(KEY_ESC, KBD_KEY_PRESSED, true);
        key(KEY_ESC, KBD_KEY_RELEASED, true);
        key(KEY_TAB, KBD_KEY_PRESSED, true);
        key(KEY_TAB, KBD_KEY_RELEASED, true);
        key(KEY_ENTER, KBD_KEY_PRESSED, true);
        key(KEY_ENTER, KBD_KEY_RELEASED, true);
        assert(s_exiting);

        advance(100);
        if (poll_during_exit) ::timer_cb(s_timer);
        assert(s_exiting && !s_panel.black && backlight_raw == 128);
        lv_anim_refr_now();
        advance(400);
        lv_anim_refr_now();
        assert(!ui_screensaver_is_active());

        // A timer firing immediately after the animation must not relock.
        ::timer_cb(s_timer);
        assert(!ui_screensaver_is_active());
        assert(lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN));
        assert(backlight_raw == 128);
        advance(9999);
        ::timer_cb(s_timer);
        assert(!ui_screensaver_is_active());
        advance(1);
        ::timer_cb(s_timer);
        assert(ui_screensaver_is_active() && s_panel.black);

        unlock_screensaver();
        ::timer_cb(s_timer);
        assert(!ui_screensaver_is_active());
        key(KEY_ESC, KBD_KEY_PRESSED, false);
        key(KEY_ESC, KBD_KEY_RELEASED, false);
    }
    screensaver_timeout = "0";
}

static void test_screensaver_black_is_independent_of_backlight()
{
    set_battery(80);
    advance(3000);
    assert(!visible());

    /* The black screen must not depend on the backlight: the simulator, web and
     * win32 backends accept the write and dim nothing, so a panel that relied on
     * it would be fully visible there.  With the write failing outright the
     * panel is still pure black and the backlight is left alone. */
    fail_backlight_write = true;
    backlight_raw = 128;
    screensaver_timeout = "10";
    advance(10000);
    ::timer_cb(s_timer);

    assert(ui_screensaver_is_active());
    assert(s_lock.state() == LockscreenState::Locked);
    assert(s_screen_off_backlight_raw < 0);
    assert(backlight_raw == 128);
    assert(s_panel.black);
    assert(s_panel.x == 0 && s_panel.y == 0);
    assert(s_panel.width == lv_display_get_horizontal_resolution(lv_display_get_default()));
    assert(s_panel.height == lv_display_get_vertical_resolution(lv_display_get_default()));
    assert(lv_color_eq(lv_obj_get_style_bg_color(s_overlay, LV_PART_MAIN), lv_color_black()));

    // Unlocking still works and reveals the wallpaper.
    fail_backlight_write = false;
    screensaver_timeout = "0";
    key(KEY_ESC, KBD_KEY_PRESSED, true);
    key(KEY_ESC, KBD_KEY_RELEASED, true);
    assert(s_lock.state() == LockscreenState::PendingUnlock);
    assert(!s_panel.black && s_panel.y == AppPageRoot::kTopBarHeightPx);
    unlock_screensaver_from_pending_unlock();
}

static void test_screensaver_battery_interaction()
{
    using namespace launcher_battery_ui;
    shutdown_warning();
    set_battery(30);
    init_warning();
    assert(!visible());
    enter_lockscreen();
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
    unlock_screensaver();
    assert(visible());
    snapshot_if_requested("BATTERY_WAKE_SNAPSHOT");
    key(KEY_ENTER, KBD_KEY_PRESSED, true);
    key(KEY_ENTER, KBD_KEY_RELEASED, true);
    assert(!visible());
    enter_lockscreen();
    advance(3000);
    unlock_screensaver();
    assert(!visible());

    // Charging and unplugging still rearm the reminder under the screensaver.
    enter_lockscreen();
    set_battery(15, true);
    advance(3000);
    set_battery(15);
    advance(3000);
    assert(flow.warning() == LowBatteryWarning::Reminder && !visible());
    unlock_screensaver();
    assert(visible());

    // Hidden warnings do not flash, steal the foreground, or pause a countdown.
    set_battery(4);
    advance(3000);
    assert(visible());
    enter_lockscreen();
    assert(!visible());
    const lv_opa_t hidden_opacity = lv_obj_get_style_bg_opa(tint, LV_PART_MAIN);
    advance(1000);
    assert(!visible() && lv_obj_get_style_bg_opa(tint, LV_PART_MAIN) == hidden_opacity);
    set_battery(3);
    advance(3000);
    assert(flow.warning() == LowBatteryWarning::ShutdownCountdown && !visible());
    advance(6000);
    unlock_screensaver();
    assert(visible());
    assert(flow.seconds_until_shutdown(lv_tick_get()) == 9);
    assert(std::strcmp(lv_label_get_text(countdown_label), "Power off in 9s") == 0);
    enter_lockscreen();
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
        unlock_screensaver();
        assert(visible() == (recovery == 2));
        enter_lockscreen();
    }
    stop_screensaver();
    shutdown_warning();
}

int main()
{
    using namespace launcher_battery_ui;
    lv_init();
    auto *display = lv_display_create(320, 170);
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
    cp0_signal_filesystem_api.append([](std::list<std::string> arguments, std::function<void(int, std::string)> reply) {
        if (reply) reply(0, arguments.back() == "lofoten_320x150.png"
                               ? TEST_LOCKSCREEN_BACKGROUND : "/missing-screensaver-test-image.png");
    });
    cp0_signal_settings_api.append([](std::list<std::string> arguments,
                                      std::function<void(int, std::string)> reply) {
        assert(!arguments.empty());
        const std::string command = arguments.front();
        if (command == "BacklightMax") {
            if (reply) reply(0, std::to_string(backlight_maximum));
        } else if (command == "BacklightRead") {
            if (reply) reply(0, std::to_string(backlight_raw));
        } else if (command == "BacklightWrite") {
            if (fail_backlight_write) {
                if (reply) reply(-1, "backlight write failed");
            } else {
                backlight_raw = std::stoi(*std::next(arguments.begin()));
                if (backlight_raw == 0) ++backlight_suspends;
                else ++backlight_restores;
                if (reply) reply(0, std::to_string(backlight_raw));
            }
        } else {
            assert(false);
        }
    });
    ui_screensaver_init();
    assert(s_timer && s_overlay);
    assert(s_background_cache.image());
    test_background_cache();

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
    unlock_screensaver();
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
    test_hold_gesture_enters_lock();
    test_screensaver_panel();
    test_unlock_restarts_idle_timeout();
    test_screensaver_black_is_independent_of_backlight();
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
