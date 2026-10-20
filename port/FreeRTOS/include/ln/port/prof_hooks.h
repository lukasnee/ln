// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

/** @brief Maps FreeRTOS trace hooks to ln::prof. Shall be included in
 * FreeRTOSConfig.h as follows:
 * @code{.c}
 *
 * #ifdef LN_PROF
 * #include "ln/port/prof_hooks.h"
 * #endif
 * @endcode
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    void ln_prof_task_switched_out(void);
    void ln_prof_task_switched_in(void);

#ifdef __cplusplus
}
#endif

#define traceTASK_SWITCHED_OUT() ln_prof_task_switched_out()
#define traceTASK_SWITCHED_IN() ln_prof_task_switched_in()
