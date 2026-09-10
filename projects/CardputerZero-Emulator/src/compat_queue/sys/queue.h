/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

// Minimal sys/queue.h polyfill for Emscripten (STAILQ only)
#ifndef _SYS_QUEUE_H_COMPAT
#define _SYS_QUEUE_H_COMPAT

#define STAILQ_HEAD(name, type) \
struct name { struct type *stqh_first; struct type **stqh_last; }

#define STAILQ_ENTRY(type) \
struct { struct type *stqe_next; }

#define STAILQ_INIT(head) do { \
    (head)->stqh_first = NULL; \
    (head)->stqh_last = &(head)->stqh_first; \
} while (0)

#define STAILQ_EMPTY(head) ((head)->stqh_first == NULL)
#define STAILQ_FIRST(head) ((head)->stqh_first)

#define STAILQ_INSERT_TAIL(head, elm, field) do { \
    (elm)->field.stqe_next = NULL; \
    *(head)->stqh_last = (elm); \
    (head)->stqh_last = &(elm)->field.stqe_next; \
} while (0)

#define STAILQ_REMOVE_HEAD(head, field) do { \
    if (((head)->stqh_first = (head)->stqh_first->field.stqe_next) == NULL) \
        (head)->stqh_last = &(head)->stqh_first; \
} while (0)

#endif
