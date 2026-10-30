// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <cstddef>

#include "FreeRTOS.h"
#include "task.h"

#ifndef LN_PORT_TASK_REGISTRY_SIZE
#define LN_PORT_TASK_REGISTRY_SIZE 16
#endif

namespace ln::port {

/**
 * @brief Copy handles of all live tasks. Allows enumerating tasks without
 * uxTaskGetSystemState(), which scans every task stack with the scheduler
 * suspended.
 *
 * @note Requires "ln/port/task_hooks.h" to be included in FreeRTOSConfig.h.
 * @note Call with the scheduler suspended or inside a critical section to get
 * a consistent list.
 *
 * @return Number of handles written to @p out (at most @p max).
 */
std::size_t task_registry_get(TaskHandle_t *out, std::size_t max);

} // namespace ln::port
