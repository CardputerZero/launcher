/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zclaw_cli_service.h"

#include <memory>

namespace zclaw {

class ProcessExecutor;

CliService make_local_cli_service(
    const std::shared_ptr<ProcessExecutor> &processes);

}  // namespace zclaw
