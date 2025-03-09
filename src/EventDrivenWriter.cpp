/*
 * fonas - C++ FreeRTOS Framework.
 * Copyright (C) 2023 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "fonas/EventDrivenWriter.hpp"

namespace fonas {

bool EventDrivenWriter::init() { return this->ll_init(); }

bool EventDrivenWriter::write(const std::uint8_t *data, std::size_t size, TickType_t timeout_ticks) {
    LockGuard lock_guard(this->mutex);
    // assure that the binary semaphore is not already given from previous timed out write() call.
    this->semaphore.Give();
    this->semaphore.Take();
    if (!this->ll_write_async(data, size)) {
        return false;
    }
    if (this->semaphore.Take(timeout_ticks) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenWriter::deinit() { return this->ll_deinit(); }

void EventDrivenWriter::ll_async_write_completed_cb_from_isr() {
    BaseType_t *pxHigherPriorityTaskWoken = nullptr;
    this->semaphore.GiveFromISR(pxHigherPriorityTaskWoken);
}

void EventDrivenWriter::ll_async_write_completed_cb() { this->semaphore.Give(); }

} // namespace fonas