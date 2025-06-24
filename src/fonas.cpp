/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "fonas/fonas.hpp"

#include "fonas/logger/logger.hpp"

namespace fonas {

LOG_MODULE(fonas, LOGGER_LEVEL_INFO);

extern "C" void fonas_panic(const char *file, int line) {
    LOG_ERROR("Panicked! at %s:%d\n", file, line);
    LOG_FLUSH();
    __asm("bkpt 1");
    while (1) {
    }
}

void delay_ms(std::uint32_t ms) {
    // cannot use cpp_freertos::Thread::Delay(ms); because it's not static method of Thread for some reason
    vTaskDelay(ms);
}
std::uint32_t get_uptime_ticks() { return static_cast<std::uint32_t>(cpp_freertos::Ticks::GetTicks()); }

std::uint32_t get_uptime_ms() {
    return static_cast<std::uint32_t>(cpp_freertos::Ticks::TicksToMs(cpp_freertos::Ticks::GetTicks()));
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
    const std::uint32_t uptime_ms = get_uptime_ms();
    const std::time_t uptime_seconds = uptime_ms / 1000;
    return {.tm = *std::gmtime(&uptime_seconds), .ms = uptime_ms % 1000};
}

} // namespace fonas
