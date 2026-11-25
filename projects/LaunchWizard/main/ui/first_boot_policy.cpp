/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "first_boot_policy.h"

namespace launch_wizard {

bool should_run_wizard(const FirstBootState &state)
{
    if (state.rearm_marker)
        return true;
    if (state.factory_marker) {
        // The baked factory password still verifying is on its own enough to
        // run the OOBE, whatever the account is called. Gating this on the
        // username as well meant a renamed account that kept the factory
        // password was treated as provisioned.
        if (state.factory_credentials)
            return true;
        return state.factory_username && !state.user_has_password;
    }
    return state.legacy_piwiz_active;
}

bool should_expand_rootfs(bool factory_marker, bool rearm_marker)
{
    if (factory_marker)
        return true;
    return !rearm_marker;
}

bool should_expand_rootfs_on_skip(bool factory_marker)
{
    return factory_marker;
}

bool should_run_keyboard_guide(bool marker_present, bool binary_present)
{
    return marker_present && binary_present;
}

bool should_consume_keyboard_guide_marker(bool exited_normally, int exit_code)
{
    return exited_normally && exit_code == 0;
}

}  // namespace launch_wizard
