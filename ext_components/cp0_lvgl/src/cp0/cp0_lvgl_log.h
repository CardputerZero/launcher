/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

void cp0_zmq_log_init(void);
void cp0_zmq_log(const char *topic, const char *message);
void cp0_zmq_logf(const char *topic, const char *fmt, ...);
