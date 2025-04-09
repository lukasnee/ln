/*
 * fonas - C++ FreeRTOS Framework.
 * Copyright (C) 2023 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "fonas/EventDrivenSpi.hpp"

namespace fonas {

bool EventDrivenSpi::init() { return this->ll_init(); }

bool EventDrivenSpi::read(std::uint8_t *data, std::size_t size, TickType_t timeout_ticks) {
    LockGuard lock_guard(this->mutex);
    // assure that the binary semaphore is not already given from previous timed out call.
    this->semaphore.Give();
    this->semaphore.Take();
    if (!this->ll_read_async(data, size)) {
        return false;
    }
    if (this->semaphore.Take(timeout_ticks) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::write(const std::uint8_t *data, std::size_t size, TickType_t timeout_ticks) {
    LockGuard lock_guard(this->mutex);
    // assure that the binary semaphore is not already given from previous timed out call.
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

bool EventDrivenSpi::read_write(std::uint8_t *rd_data, const std::uint8_t *wr_data, std::size_t size,
                                TickType_t timeout_ticks) {
    LockGuard lock_guard(this->mutex);
    // assure that the binary semaphore is not already given from previous timed out call.
    this->semaphore.Give();
    this->semaphore.Take();
    if (!this->ll_read_write_async(rd_data, wr_data, size)) {
        return false;
    }
    if (this->semaphore.Take(timeout_ticks) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::deinit() { return this->ll_deinit(); }

void EventDrivenSpi::ll_async_complete_common_signal() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const auto is_inside_interrupt = xPortIsInsideInterrupt();
    if (is_inside_interrupt) {
        this->semaphore.GiveFromISR(&xHigherPriorityTaskWoken);
    }
    else {
        this->semaphore.Give();
    }
    if (is_inside_interrupt) {
        portYIELD_FROM_ISR(&xHigherPriorityTaskWoken);
    }
    // portYIELD_FROM_ISR must be called the last here.
}

void EventDrivenSpi::ll_async_read_completed_cb() { this->ll_async_complete_common_signal(); }
void EventDrivenSpi::ll_async_write_completed_cb() { this->ll_async_complete_common_signal(); }
void EventDrivenSpi::ll_async_read_write_completed_cb() { this->ll_async_complete_common_signal(); }
void EventDrivenSpi::ll_async_abnormal_cb() { this->ll_async_complete_common_signal(); }
} // namespace fonas
