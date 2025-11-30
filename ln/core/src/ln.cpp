/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/ln.hpp"

#include "ln/logger/logger.hpp"

#include "FreeRTOS/Addons/Clock.hpp"

namespace ln {

LOG_MODULE(ln, LOGGER_LEVEL_INFO);

extern "C" void ln_panic(const char *file, int line) {
    LOG_ERROR("Panicked! at %s:%d\n", file, line);
    LOG_FLUSH();
    __asm("bkpt 1");
    while (1) {
    }
}

bool is_inside_interrupt() { return xPortIsInsideInterrupt() != pdFALSE; }

const char *get_current_thread_name() {
    const char *name = pcTaskGetName(nullptr);
    if (!name) {
        return "Unknown";
    }
    return name;
}

} // namespace ln
