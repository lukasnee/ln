// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/port/task_registry.hpp"

#include "ln/port/task_hooks.h"

namespace {

// Hooks are invoked by the kernel inside a critical section.
TaskHandle_t registry[LN_PORT_TASK_REGISTRY_SIZE];

} // namespace

extern "C" void ln_port_task_created(void *tcb) {
    for (auto &slot : registry) {
        if (slot == nullptr) {
            slot = static_cast<TaskHandle_t>(tcb);
            return;
        }
    }
}

extern "C" void ln_port_task_deleted(void *tcb) {
    for (auto &slot : registry) {
        if (slot == tcb) {
            slot = nullptr;
            return;
        }
    }
}

namespace ln::port {

std::size_t task_registry_get(TaskHandle_t *out, std::size_t max) {
    std::size_t n = 0;
    for (auto handle : registry) {
        if (handle != nullptr && n < max) {
            out[n++] = handle;
        }
    }
    return n;
}

} // namespace ln::port
