/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_esc_exit_policy.hpp"

#include <cassert>

int main()
{
    cp0_esc_exit_policy::StateMachine policy;
    assert(policy.timeout_ms(999) == -1);
    auto decision = policy.update(1000, true);
    assert(policy.timeout_ms(1000) == 500);
    assert(!decision.show_hint && !decision.send_terminate);
    decision = policy.update(1499, true);
    assert(!decision.show_hint && !decision.send_terminate);
    assert(policy.timeout_ms(1500) == 0);
    decision = policy.update(1500, true);
    assert(policy.timeout_ms(1500) == 2500);
    assert(decision.show_hint && !decision.send_terminate);
    decision = policy.update(3999, true);
    assert(!decision.send_terminate);
    decision = policy.update(4000, true);
    assert(decision.send_terminate && !decision.send_kill);
    assert(policy.timeout_ms(4000) == 3000);
    decision = policy.update(6999, true);
    assert(!decision.send_kill);
    decision = policy.update(7000, true);
    assert(decision.send_kill);
    assert(policy.timeout_ms(7000) == -1);

    cp0_esc_exit_policy::StateMachine released;
    released.update(0, true);
    decision = released.update(500, true);
    assert(decision.show_hint);
    decision = released.update(501, false);
    assert(decision.hide_hint);
    assert(released.timeout_ms(501) == -1);
    released.update(1000, true);
    decision = released.update(3499, true);
    assert(!decision.send_terminate);

    cp0_esc_exit_policy::StateMachine graceful;
    graceful.update(0, true);
    graceful.update(3000, true);
    decision = graceful.update(5500, true);
    assert(!decision.send_kill);
    decision = graceful.finish();
    assert(decision.hide_hint);
}
