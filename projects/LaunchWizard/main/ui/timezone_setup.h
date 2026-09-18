/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#ifndef LAUNCH_WIZARD_TIMEZONE_SETUP_H
#define LAUNCH_WIZARD_TIMEZONE_SETUP_H

#include "command_runner.h"
#include "wizard_model.h"

namespace launch_wizard {

using TimezoneCommandRunner = std::function<CommandResult(
    const std::vector<std::string> &, const std::string *)>;

std::string apply_timezone(const Timezone &timezone, TimezoneMode mode,
                           const TimezoneCommandRunner &run);

} // namespace launch_wizard

#endif
