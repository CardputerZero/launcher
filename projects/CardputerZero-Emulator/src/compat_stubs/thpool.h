/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <stddef.h>
typedef void *threadpool;
static inline threadpool thpool_init(int num) { (void)num; return NULL; }
static inline int thpool_add_work(threadpool tp, void (*fn)(void*), void *arg) { (void)tp; if(fn) fn(arg); return 0; }
static inline void thpool_destroy(threadpool tp) { (void)tp; }
