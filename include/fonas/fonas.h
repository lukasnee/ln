/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <stdio.h> // for printf
#include <string.h> // for strrchr

#ifdef __cplusplus
extern "C"
{
#endif

#define LOG(...) printf(__VA_ARGS__)

#if DEBUG
#define LOGD(...) printf(__VA_ARGS__)
#else
#define LOGD(...)
#endif

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define PANIC()                                                                                                        \
    LOG("Panic! at %s:%d", __FILENAME__, __LINE__);                                                                    \
    __asm("bkpt 1");                                                                                                   \
    while (1) {                                                                                                        \
    }

#define ASSERT(expr)                                                                                                   \
    if (!(expr)) {                                                                                                     \
        PANIC();                                                                                                       \
    }

#ifdef __cplusplus
}
#endif
