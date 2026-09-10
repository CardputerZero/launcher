/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "emu_compat.h"
#ifdef _WIN32
#include "compat/win32_stubs.h"
#endif

// MinGW doesn't properly link __attribute__((weak)) functions.
// Replace weak with a no-op via macro so they become strong definitions.
#if defined(__MINGW32__) && !defined(__clang__)
#define weak
#endif
