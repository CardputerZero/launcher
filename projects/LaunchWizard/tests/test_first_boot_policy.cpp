/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "first_boot_policy.h"

#include <iostream>
#include <string>

namespace {

bool expect_wizard(bool expected, const launch_wizard::FirstBootState &state,
                   const std::string &label)
{
    const bool actual = launch_wizard::should_run_wizard(state);
    if (actual == expected)
        return true;
    std::cerr << "should_run_wizard mismatch (" << label << "): expected "
              << expected << ", got " << actual << "\n";
    return false;
}

}  // namespace

bool test_first_boot_policy()
{
    bool passed = true;
    launch_wizard::FirstBootState state;

    // Factory first boot: marker present, default username, no password ->
    // wizard.
    state.factory_marker = true;
    state.factory_username = true;
    passed &= expect_wizard(true, state, "factory unconfigured");

    // Factory image with the baked pi/raspberry password: the hash exists but
    // still verifies as the factory default, so the wizard must run.
    state.user_has_password = true;
    state.factory_credentials = true;
    passed &= expect_wizard(true, state, "factory baked default password");

    // Imager-provisioned device: factory marker still present, but the user
    // chose their own password -> skip straight to the launcher (bug #227).
    state.factory_credentials = false;
    passed &= expect_wizard(false, state, "factory imager-provisioned");

    // A renamed user means the device was provisioned, even when no password
    // was set (e.g. Imager with SSH keys only).
    state.factory_username = false;
    state.user_has_password = false;
    passed &= expect_wizard(false, state, "renamed user without password");

    // ...but a rename does not re-hash the password. While the baked default
    // still verifies, the shipped credential is live and the OOBE must run so
    // the user is made to replace it. Previously the username check
    // short-circuited this, and first_user_has_factory_credentials() refused to
    // hash a renamed account at all, so this state was unreachable.
    state.user_has_password = true;
    state.factory_credentials = true;
    passed &= expect_wizard(true, state, "renamed user keeps factory password");

    // Settings "Run Setup Wizard" re-arm always wins, even when configured.
    state.rearm_marker = true;
    passed &= expect_wizard(true, state, "re-arm on configured device");

    // No markers at all: fall back to the legacy piwiz/lightdm signal.
    state = {};
    passed &= expect_wizard(false, state, "no markers");
    state.legacy_piwiz_active = true;
    passed &= expect_wizard(true, state, "legacy piwiz");
    state.user_has_password = true;
    passed &= expect_wizard(true, state, "legacy piwiz ignores password");

    // Root expansion runs on the genuine first boot only; a settings-driven
    // re-run must not rewrite an already-expanded card's partition table, but a
    // re-arm that races an unfinished first boot must still expand it.
    passed &= launch_wizard::should_expand_rootfs(false, false);
    passed &= launch_wizard::should_expand_rootfs(true, false);
    passed &= launch_wizard::should_expand_rootfs(true, true);
    passed &= !launch_wizard::should_expand_rootfs(false, true);

    // The skip path expands the card itself, because the OOBE (and its apply
    // pipeline) never runs there. Only the factory marker may trigger it, so a
    // skipped boot on an already-expanded card leaves the partition table alone.
    passed &= launch_wizard::should_expand_rootfs_on_skip(true);
    passed &= !launch_wizard::should_expand_rootfs_on_skip(false);

    // Keyboard guide: both marker and binary are required. In particular, a
    // missing package must not consume the marker.
    passed &= launch_wizard::should_run_keyboard_guide(true, true);
    passed &= !launch_wizard::should_run_keyboard_guide(true, false);
    passed &= !launch_wizard::should_run_keyboard_guide(false, true);
    passed &= !launch_wizard::should_run_keyboard_guide(false, false);

    // An interrupted guide must remain armed for the next boot; only a normal
    // zero exit consumes the one-shot marker.
    passed &= launch_wizard::should_consume_keyboard_guide_marker(true, 0);
    passed &= !launch_wizard::should_consume_keyboard_guide_marker(true, 1);
    passed &= !launch_wizard::should_consume_keyboard_guide_marker(false, 0);

    if (!passed)
        std::cerr << "first boot policy tests failed\n";
    return passed;
}
