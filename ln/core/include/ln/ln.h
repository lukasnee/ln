/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LN_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

    void ln_panic(const char *file, int line);

#define LN_PANIC() ln_panic(LN_FILENAME, __LINE__)

#define LN_ASSERT(expr, on_failure)                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            on_failure;                                                                                                \
        }                                                                                                              \
    } while (0)
#define LN_ASSERT_PANIC(expr) LN_ASSERT(expr, LN_PANIC())

#define LN_CHECK(expr, var, cond, on_failure, on_success)                                                              \
    do {                                                                                                               \
        const auto var = (expr);                                                                                       \
        if (cond) {                                                                                                    \
            on_failure;                                                                                                \
            return var;                                                                                                \
        }                                                                                                              \
        on_success;                                                                                                    \
    } while (0)

#ifdef __cplusplus
}
#endif
