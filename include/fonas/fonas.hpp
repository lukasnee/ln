/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "fonas/fonas.h"

#include "mutex.hpp"
#include "queue.hpp"
#include "semaphore.hpp"
#include "thread.hpp"
#include "ticks.hpp"
#include "timeout.hpp"

#include <cstdint>
#include <ctime>

namespace fonas {

using cpp_freertos::BinarySemaphore;
using cpp_freertos::LockGuard;
using cpp_freertos::MutexRecursive;
using cpp_freertos::MutexStandard;
using cpp_freertos::Queue;
using cpp_freertos::Thread;
using cpp_freertos::Timeout;

void delay_ms(std::uint32_t ms);
std::uint32_t get_uptime_ticks();
std::uint32_t get_uptime_ms();

const char* get_current_thread_name();

struct Timestamp {
    std::tm tm;
    std::uint32_t ms;
};

Timestamp get_timestamp();

} // namespace fonas

constexpr TickType_t operator"" _ticks(unsigned long long ticks) { return static_cast<TickType_t>(ticks); }
