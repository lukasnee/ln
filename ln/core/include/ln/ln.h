/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <string.h> // for strrchr

#ifdef __cplusplus
extern "C"
{
#endif

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

    void ln_panic(const char *file, int line);

#define LN_PANIC() ln_panic(__FILENAME__, __LINE__)

#define LN_ASSERT(expr)                                                                                                \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            LN_PANIC();                                                                                                \
        }                                                                                                              \
    } while (0)

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
