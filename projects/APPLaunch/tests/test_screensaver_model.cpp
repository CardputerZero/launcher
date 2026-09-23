/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../main/ui/model/screensaver_model.hpp"
#include "../main/ui/model/screensaver_runtime_contract.hpp"

#include "input_keys.h"

#include <cassert>
#include <cstdint>

int main()
{
    int timer = 0;
    int stale_timer = 0;
    assert(screensaver_timer_is_current(&timer, &timer));
    assert(!screensaver_timer_is_current(&stale_timer, &timer));
    assert(!screensaver_timer_is_current(nullptr, &timer));
    assert(screensaver_delete_is_tracked(&timer, &timer, &timer));
    assert(!screensaver_delete_is_tracked(&stale_timer, &timer, &timer));
    assert(!screensaver_delete_is_tracked(&timer, &stale_timer, &timer));
    assert(!screensaver_delete_is_tracked(&timer, &timer, &stale_timer));
    assert(!screensaver_delete_is_tracked(nullptr, &timer, &timer));
    assert(screensaver_timeout_from_config(true, "0") == 0);
    assert(screensaver_timeout_from_config(true, "10") == 10);
    assert(screensaver_timeout_from_config(true, "300") == 300);
    assert(screensaver_timeout_from_config(true, "garbage") == 30);
    assert(screensaver_timeout_from_config(true, "10junk") == 30);
    assert(screensaver_timeout_from_config(true, " 10") == 30);
    assert(screensaver_timeout_from_config(true, "+10") == 30);
    assert(screensaver_timeout_from_config(true, "99999999999999999999") == 30);
    assert(screensaver_timeout_from_config(true, "-1") == 30);
    assert(screensaver_timeout_from_config(false, "0") == 30);

    ScreensaverModel model;
    model.reset(1000);
    assert(!model.active());
    assert(!model.should_activate(30999, 30000, true));
    assert(model.should_activate(31000, 30000, true));
    assert(!model.should_activate(31000, 0, true));
    assert(!model.should_activate(31000, 30000, false));

    model.activate();
    assert(model.active());
    assert(!model.should_activate(61000, 30000, true));

    // While the screensaver is up the model only records activity; the lock
    // screen owns every key, so nothing here clears the active flag.
    model.note_activity(40020);
    assert(model.last_activity_tick() == 40020);
    assert(model.active());

    model.set_foreground(false, 60000);
    assert(!model.foreground());
    assert(!model.active());
    assert(!model.should_activate(90000, 30000, true));
    model.set_foreground(true, 90000);
    assert(!model.should_activate(119999, 30000, true));
    assert(model.should_activate(120000, 30000, true));

    model.reset(UINT32_MAX - 10);
    assert(model.should_activate(9, 20, true));

    // ---- Long-press gesture that requests the lock ----
    ScreensaverModel hold;
    assert(ScreensaverModel::screen_off_hold_ms() == 3000);
    assert(ScreensaverModel::hold_hint_ms() == 500);
    hold.reset(1000);
    assert(!hold.hold_pending());
    assert(!hold.hold_hint_visible());
    assert(!hold.poll_hold(500000).fire);

    ScreensaverHoldDecision decision = hold.observe_hold_key(KEY_TAB, false, 2000);
    assert(!decision.show_hint && !decision.hide_hint && !decision.fire);
    assert(hold.hold_pending());
    // The gesture announces itself once, before it fires.
    assert(!hold.poll_hold(2499).show_hint);
    decision = hold.poll_hold(2500);
    assert(decision.show_hint && !decision.fire);
    assert(hold.hold_hint_visible());
    assert(!hold.poll_hold(4000).show_hint);
    // Auto-repeat must not restart the window, or a held key never matures.
    assert(!hold.observe_hold_key(KEY_TAB, false, 4000).hide_hint);
    assert(!hold.poll_hold(4999).fire);
    decision = hold.poll_hold(5000);
    assert(decision.fire && !decision.show_hint);
    // The threshold reports once per hold, and entering the lock clears it.
    assert(!hold.poll_hold(9000).fire);
    hold.activate();
    assert(hold.active() && !hold.hold_pending() && !hold.hold_hint_visible());

    // A short tap never requests anything and never shows the hint.
    hold.observe_hold_key(KEY_TAB, false, 100);
    hold.observe_hold_key(KEY_TAB, true, 200);
    assert(!hold.hold_pending() && !hold.hold_hint_visible());
    assert(!hold.poll_hold(500000).fire);

    // Releasing clears a visible hint.
    hold.observe_hold_key(KEY_TAB, false, 100);
    assert(hold.poll_hold(600).show_hint && hold.hold_hint_visible());
    decision = hold.observe_hold_key(KEY_TAB, true, 700);
    assert(decision.hide_hint && !hold.hold_pending() && !hold.hold_hint_visible());

    // Another key cancels the gesture and clears its hint.
    hold.observe_hold_key(KEY_TAB, false, 100);
    assert(hold.poll_hold(600).show_hint);
    decision = hold.observe_hold_key(KEY_ENTER, false, 700);
    assert(decision.hide_hint && !hold.hold_pending() && !hold.hold_hint_visible());
    assert(!hold.poll_hold(500000).fire);

    // Leaving the lock clears a pending gesture along with the active flag.
    hold.observe_hold_key(KEY_TAB, false, 10);
    hold.deactivate();
    assert(!hold.hold_pending() && !hold.active());
    hold.observe_hold_key(KEY_TAB, false, 20);
    hold.set_foreground(false, 30);
    assert(!hold.hold_pending() && !hold.active() && !hold.foreground());
}
