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

#include <algorithm> // std::min

namespace fonas {

bool EventDrivenSpi::init() { return this->ll_init(); }

bool EventDrivenSpi::ll_ensure_write_readiness(fonas::Timeout timeout) {
    while (!timeout.Expired()) {
        if (!this->ll_busy_writing()) {
            return true;
        }
        if (this->write_semaphore.Take(std::min(timeout.Left(), 10_ticks)) == pdTRUE && !this->ll_busy_writing()) {
            return true;
        }
    }
    return false;
}

bool EventDrivenSpi::ll_ensure_read_readiness(fonas::Timeout timeout) {
    while (!timeout.Expired()) {
        if (!this->ll_busy_reading()) {
            return true;
        }
        if (this->read_semaphore.Take(std::min(timeout.Left(), 10_ticks)) == pdTRUE && !this->ll_busy_reading()) {
            return true;
        }
    }
    return false;
}

bool EventDrivenSpi::read(std::uint8_t *data, std::size_t size, fonas::Timeout timeout) {
    if (size == 0) {
        return true;
    }
    if (!data) {
        return false;
    }
    LockGuard lock_guard(this->mutex);
    if (!this->ll_ensure_read_readiness(timeout)) {
        return false;
    }
    if (!this->ll_read_async(data, size)) {
        return false;
    }
    if (this->read_semaphore.Take(timeout.Left()) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::write(const std::uint8_t *data, std::size_t size, fonas::Timeout timeout) {
    if (size == 0) {
        return true;
    }
    if (!data) {
        return false;
    }
    LockGuard lock_guard(this->mutex);
    if (!this->ll_ensure_write_readiness(timeout)) {
        return false;
    }
    if (!this->ll_write_async(data, size)) {
        return false;
    }
    if (this->write_semaphore.Take(timeout.Left()) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::write_async(const std::uint8_t *data, std::size_t size, fonas::Timeout timeout) {
    if (size == 0) {
        return true;
    }
    if (!data) {
        return false;
    }
    LockGuard lock_guard(this->mutex);
    if (!this->ll_ensure_write_readiness(timeout)) {
        return false;
    }
    if (!this->ll_write_async(data, size)) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::write_await(fonas::Timeout timeout) {
    LockGuard lock_guard(this->mutex);
    if (!this->ll_busy_writing()) {
        return true;
    }
    if (this->write_semaphore.Take(timeout.Left()) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::read_write(std::uint8_t *rd_data, const std::uint8_t *wr_data, std::size_t size,
                                fonas::Timeout timeout) {
    if (size == 0) {
        return true;
    }
    if (!rd_data) {
        return false;
    }
    if (!wr_data) {
        return false;
    }
    LockGuard lock_guard(this->mutex);
    if (!this->ll_ensure_read_readiness(timeout)) {
        return false;
    }
    if (!this->ll_ensure_write_readiness(timeout)) {
        return false;
    }
    if (!this->ll_read_write_async(rd_data, wr_data, size)) {
        return false;
    }
    if (this->read_semaphore.Take(timeout.Left()) != pdTRUE) {
        return false;
    }
    if (this->write_semaphore.Take(timeout.Left()) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::deinit() { return this->ll_deinit(); }

void EventDrivenSpi::ll_async_complete_common_signal() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const auto is_inside_interrupt = xPortIsInsideInterrupt();
    if (is_inside_interrupt) {
        this->write_semaphore.GiveFromISR(&xHigherPriorityTaskWoken);
    }
    else {
        this->write_semaphore.Give();
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
