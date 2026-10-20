// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/Clock.hpp"

#include "FreeRTOS.h"

#include <chrono>

namespace ln::port {

inline TickType_t to_ticks(const std::chrono::milliseconds &timeout) {
    if (timeout >= Clock::duration::max()) {
        return portMAX_DELAY;
    }
    return pdMS_TO_TICKS(timeout.count());
}

} // namespace ln::port
