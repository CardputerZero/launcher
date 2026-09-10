/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace cp0_lora_hat_power_controller {

bool enable(int fallback_gpio);
void shutdown();

} // namespace cp0_lora_hat_power_controller
