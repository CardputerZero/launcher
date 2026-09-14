/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

// Stub sys/wait.h for Windows
#ifndef _SYS_WAIT_COMPAT_H
#define _SYS_WAIT_COMPAT_H
#ifdef _WIN32
#include <stdlib.h>
typedef int pid_t;
#define WEXITSTATUS(s) ((s) & 0xff)
#define WIFEXITED(s) 1
#define WIFSIGNALED(s) 0
static inline pid_t fork(void) { return -1; }
static inline pid_t waitpid(pid_t p, int *s, int o) { (void)p;(void)s;(void)o; return -1; }
static inline int kill(pid_t p, int s) { (void)p;(void)s; return -1; }
static inline void _exit(int s) { exit(s); }
#ifndef SIGTERM
#define SIGTERM 15
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#endif
#endif
