// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/ln.h"
#include "ln/MutexI.hpp"
#include "ln/Clock.hpp"

#include <fmt/core.h>

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <thread>

namespace ln {

const auto g_start_time = std::chrono::steady_clock::now();

Clock::time_point Clock::now() noexcept {
    return Clock::time_point{std::chrono::duration_cast<Clock::duration>(
        std::chrono::steady_clock::now() - g_start_time)};
}

std::chrono::milliseconds MutexI::max_timeout() {
    return std::chrono::milliseconds::max();
}

void panic_port(const char *file, int line, const char *message) {
    fmt::print("\n{}:{}: panic{}{}\n", file, line, (message ? ": " : "!"),
               (message ? message : ""));
}

void reset() { std::abort(); }

std::chrono::milliseconds get_uptime_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now().time_since_epoch());
}

void sleep(std::chrono::milliseconds duration) {
    std::this_thread::sleep_for(duration);
}

bool interrupt_context() { return false; }

std::string_view get_thread_name() { return {}; }

} // namespace ln
