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

Timestamp get_timestamp() {
    using namespace std::chrono;
    const auto uptime_ms = duration_cast<milliseconds>(FreeRTOS::Addons::Clock::now().time_since_epoch()).count();
    const std::time_t uptime_seconds = uptime_ms / 1000;
    return {.tm = *std::gmtime(&uptime_seconds), .ms = static_cast<std::uint32_t>(uptime_ms % 1000)};
}

} // namespace ln
