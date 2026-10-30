// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

/** @brief Maps FreeRTOS trace hooks to the ln task registry. Shall be included
 * in FreeRTOSConfig.h as follows:
 * @code{.c}
 *
 * #include "ln/port/task_hooks.h"
 * @endcode
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    void ln_port_task_created(void *tcb);
    void ln_port_task_deleted(void *tcb);

#ifdef __cplusplus
}
#endif

#define traceTASK_CREATE(pxNewTCB) ln_port_task_created(pxNewTCB)
#define traceTASK_DELETE(pxTaskToDelete) ln_port_task_deleted(pxTaskToDelete)
