/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "FreeRTOS/Addons/Clock.hpp"
#include "FreeRTOS/Addons/LockGuard.hpp"
#include "FreeRTOS/Addons/Timeout.hpp"

#include <ctime>

namespace ln {

using Clock = FreeRTOS::Addons::Clock;
using LockGuard = FreeRTOS::Addons::LockGuard;
using Timeout = FreeRTOS::Addons::Timeout;

bool is_inside_interrupt();
const char *get_current_thread_name();

} // namespace ln
