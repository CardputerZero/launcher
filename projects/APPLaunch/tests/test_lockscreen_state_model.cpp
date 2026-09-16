/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../main/ui/model/lockscreen_state_model.hpp"

#include <cassert>
#include <cstdint>

namespace {

/* A fresh press followed by its release, the way the key queue delivers them. */
LockscreenDecision press(LockscreenStateModel &model, uint32_t key_code, uint32_t now)
{
    const LockscreenDecision decision = model.handle_key(key_code, true, false, now);
    const LockscreenDecision release = model.handle_key(key_code, false, true, now + 1);
    assert(release.consumed);
    assert(!release.show_panel && !release.sleep && !release.unlock);
    assert(!release.blocked);
    return decision;
}

} // namespace

int main()
{
    LockscreenStateModel model;
    assert(LockscreenStateModel::timeout_ms() == 10000);
    model.reset(1000);
    assert(model.state() == LockscreenState::Locked);
    assert(!model.visible());

    // (1) -> (2): any key brings the visible screen up, and is consumed.
    LockscreenDecision decision = press(model, KEY_ESC, 2000);
    assert(decision.consumed && decision.show_panel);
    assert(!decision.sleep && !decision.unlock);
    assert(!decision.blocked);
    assert(model.state() == LockscreenState::PendingUnlock);
    assert(model.visible());

    // (2) waits for the gesture: any other key only counts as activity.
    decision = press(model, KEY_ESC, 3000);
    assert(decision.consumed && decision.blocked);
    assert(!decision.show_panel && !decision.sleep && !decision.unlock);
    assert(model.state() == LockscreenState::PendingUnlock);

    // (2) -> (3) on TAB, and the panel is refreshed for the new hint.
    decision = press(model, KEY_TAB, 4000);
    assert(decision.consumed && decision.show_panel && !decision.blocked);
    assert(model.state() == LockscreenState::Armed);

    // (3) -> (2) on any key that is not a confirmation.
    decision = press(model, KEY_TAB, 5000);
    assert(decision.consumed && decision.show_panel && decision.blocked);
    assert(!decision.unlock);
    assert(model.state() == LockscreenState::PendingUnlock);

    decision = press(model, KEY_TAB, 6000);
    assert(model.state() == LockscreenState::Armed);

    // (3) + ENTER leaves the lock screen; the caller resets for the next entry.
    decision = press(model, KEY_ENTER, 7000);
    assert(decision.consumed && decision.unlock);
    assert(!decision.show_panel && !decision.sleep && !decision.blocked);
    model.reset(7001);
    assert(model.state() == LockscreenState::Locked);

    // The keypad Enter confirms as well.
    decision = press(model, KEY_ESC, 8000);
    assert(model.state() == LockscreenState::PendingUnlock);
    decision = press(model, KEY_TAB, 8001);
    assert(model.state() == LockscreenState::Armed);
    decision = press(model, KEY_KPENTER, 8002);
    assert(decision.unlock);

    // Auto-repeat is activity only: holding TAB cannot walk (2) into (3), and
    // holding a key in (3) cannot keep stepping the machine.
    model.reset(20000);
    decision = press(model, KEY_TAB, 20000);
    assert(model.state() == LockscreenState::PendingUnlock);
    for (uint32_t step = 0; step < 40; ++step) {
        const LockscreenDecision repeat = model.handle_key(KEY_TAB, false, false, 20100 + step * 50);
        assert(repeat.consumed);
        assert(!repeat.show_panel && !repeat.sleep && !repeat.unlock);
        assert(!repeat.blocked);
    }
    assert(model.state() == LockscreenState::PendingUnlock);
    // The held key's release must not advance the machine either.
    const LockscreenDecision held_release = model.handle_key(KEY_TAB, false, true, 23000);
    assert(held_release.consumed && !held_release.show_panel && !held_release.unlock);
    assert(model.state() == LockscreenState::PendingUnlock);

    // (2) times out back to (1) after 10 s of no input.  The repeats above count
    // as activity, so the countdown is measured from the last one (22050).
    assert(!model.poll(32049).sleep);
    assert(model.poll(32050).sleep);
    assert(model.state() == LockscreenState::Locked);
    assert(!model.visible());
    // Nothing to do while already locked.
    assert(!model.poll(90000).sleep);

    decision = press(model, KEY_ESC, 100000);
    assert(model.state() == LockscreenState::PendingUnlock);
    assert(!model.poll(109999).sleep);
    // A press resets the countdown, even one that does not change state.
    const LockscreenDecision activity = model.handle_key(KEY_ESC, true, false, 105000);
    assert(activity.consumed && !activity.show_panel);
    assert(!model.poll(114999).sleep);
    assert(model.poll(115000).sleep);

    // (3) times out the same way.
    model.reset(200000);
    press(model, KEY_ESC, 200000);
    assert(model.state() == LockscreenState::PendingUnlock);
    press(model, KEY_TAB, 200001);
    assert(model.state() == LockscreenState::Armed);
    assert(!model.poll(209999).sleep);
    assert(!model.poll(210000).sleep);
    const LockscreenDecision armed_sleep = model.poll(210001);
    assert(armed_sleep.sleep && armed_sleep.consumed);
    assert(model.state() == LockscreenState::Locked);

    // Tick arithmetic must survive a wrap: a press just before the wrap and a
    // poll just after it is a short interval, not a huge one.
    model.reset(UINT32_MAX - 5);
    press(model, KEY_ESC, UINT32_MAX - 5);
    assert(!model.poll(4).sleep);
    assert(model.poll(9994).sleep);
}
