/* SPDX-License-Identifier: MIT */
#include "../../main/ui/settings/settings_bluetooth_page.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>

decltype(cp0_signal_bt_api) cp0_signal_bt_api;
decltype(cp0_signal_bt_agent) cp0_signal_bt_agent;
volatile uint32_t LV_EVENT_KEYBOARD = 0;
static cp0_keyboard_input_context_t input_context = KBD_INPUT_CONTEXT_NAVIGATION;
static int intercept = 0;
extern "C" void cp0_keyboard_set_lvgl_keypad_intercept(int value) { intercept = value; }
extern "C" int cp0_keyboard_get_lvgl_keypad_intercept() { return intercept; }
extern "C" void cp0_keyboard_set_input_context(cp0_keyboard_input_context_t value) { input_context = value; }
extern "C" cp0_keyboard_input_context_t cp0_keyboard_get_input_context() { return input_context; }
bool settings_bluetooth_named_only_enabled() { return false; }

static void snapshot(const char *name)
{
    const char *directory = std::getenv("BLUETOOTH_UI_SNAPSHOT_DIR");
    if (!directory) return;
    lv_obj_update_layout(lv_screen_active());
    auto *buffer = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    assert(buffer);
    auto *file = std::fopen((std::string(directory) + "/" + name + ".ppm").c_str(), "wb");
    assert(file);
    std::fprintf(file, "P6\n%u %u\n255\n", buffer->header.w, buffer->header.h);
    for (unsigned y = 0; y < buffer->header.h; ++y) {
        const auto *row = buffer->data + y * buffer->header.stride;
        for (unsigned x = 0; x < buffer->header.w; ++x) {
            const unsigned char rgb[] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
            std::fwrite(rgb, 1, 3, file);
        }
    }
    std::fclose(file);
    lv_draw_buf_destroy(buffer);
}

static void key(uint32_t code, int state = KBD_KEY_PRESSED)
{
    key_item item{};
    item.key_code = code;
    item.key_state = state;
    lv_obj_send_event(lv_screen_active(), static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), &item);
}

int main()
{
    lv_init();
    auto *display = lv_display_create(320, 150);
    LV_EVENT_KEYBOARD = lv_event_register_id();
    std::mutex mutex;
    std::vector<std::string> calls;
    std::vector<std::function<void(int, std::string)>> delayed;
    std::function<void(int, std::string)> reset_done;
    bool fail_stop = true;
    cp0_signal_bt_api.append([&](std::list<std::string> args, auto done) {
        std::lock_guard<std::mutex> lock(mutex);
        calls.push_back(args.front());
        if (args.front() == "BtStatus") done(0, "1\t11:22:33:44:55:66\t0\tTest");
        else if (args.front() == "BtDiscoveryStop") done(fail_stop ? -1 : 0, {});
        else if (args.front() == "BtReset") reset_done = std::move(done);
        else if (args.front() == "BtPair") delayed.push_back(std::move(done));
        else done(0, {});
    });
    {
        LvSettingBluetoothPage3 page;
        page.mode_ = LvSettingBluetoothListMode::Scan;
        page.create_ui(lv_screen_active());
        auto drain = [&] {
            for (int i = 0; i < 5; ++i) {
                page.api_tasks_.join_all();
                page.api_result_timer_cb(page.api_timer_);
            }
        };
        drain();
        assert(page.discovery_active_);
        key(KEY_R);
        drain();
        assert(page.error_message_ == "Bluetooth scan stop failed.");
        assert(page.scan_stop_required_);
        snapshot("bluetooth-error");
        calls.clear();
        key(KEY_R);
        drain();
        assert((calls == std::vector<std::string>{"BtStatus", "BtDiscoveryStop"}));

        // B must interrupt pending work, rather than being gated by action flags.
        page.request_api({"BtPair"}, [&](int, std::string) { assert(false); },
                         [&](int, std::string) { assert(false); });
        page.api_tasks_.join_all();
        page.action_pending_ = true;
        page.pair_in_flight_ = true;
        key(KEY_B);
        drain();
        assert(page.reset_pending_ && !page.action_pending_);
        assert(reset_done);
        snapshot("bluetooth-resetting");
        const auto call_count = calls.size();
        key(KEY_B, KBD_KEY_REPEATED);
        key(KEY_B, KBD_KEY_RELEASED);
        key(KEY_B);
        drain();
        assert(calls.size() == call_count);
        delayed.front()(-1, "late pairing failure");
        drain();
        assert(page.reset_pending_ && page.error_message_.empty());
        fail_stop = false;
        reset_done(0, {});
        drain();
        assert(!page.reset_pending_ && page.discovery_active_ && page.error_message_.empty());
        snapshot("bluetooth-recovered");

        // Failure leaves B available for another reset, including a stale off state.
        page.powered_ = false;
        key(KEY_B);
        drain();
        reset_done(-1, "Bluetooth power-on failed.");
        drain();
        assert(!page.reset_pending_ && !page.error_message_.empty());
        key(KEY_B);
        drain();
        assert(page.reset_pending_);
        reset_done(0, {});
        drain();
        assert(page.discovery_active_ && page.powered_);
    }
    lv_display_delete(display);
    lv_deinit();
}
