/*
 * Copyright (C) 2023 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/drivers/EventDrivenSpi.hpp"

#include <FreeRTOS/Addons/LockGuard.hpp>

#include <algorithm> // std::min

namespace ln::drivers {

using namespace std::chrono_literals;

bool EventDrivenSpi::init() { return this->ll_init(); }

bool EventDrivenSpi::ll_ensure_write_readiness(const Timeout &timeout) {
    while (!timeout.is_expired()) {
        if (!this->ll_busy_writing()) {
            return true;
        }
        if (this->write_semaphore.take(std::min(timeout.left(), {10ms})) == pdTRUE && !this->ll_busy_writing()) {
            return true;
        }
    }
    return false;
}

bool EventDrivenSpi::ll_ensure_read_readiness(const Timeout &timeout) {
    while (!timeout.is_expired()) {
        if (!this->ll_busy_reading()) {
            return true;
        }
        if (this->read_semaphore.take(std::min(timeout.left(), {10ms})) == pdTRUE && !this->ll_busy_reading()) {
            return true;
        }
    }
    return false;
}

bool EventDrivenSpi::read(std::uint8_t *data, std::size_t size, const Timeout &timeout) {
    if (size == 0) {
        return true;
    }
    if (!data) {
        return false;
    }
    FreeRTOS::Addons::LockGuard lock_guard(this->mutex);
    if (!this->ll_ensure_read_readiness(timeout)) {
        return false;
    }
    if (!this->ll_read_async(data, size)) {
        return false;
    }
    if (this->read_semaphore.take(timeout.left()) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::write(const std::uint8_t *data, std::size_t size, const Timeout &timeout) {
    if (size == 0) {
        return true;
    }
    if (!data) {
        return false;
    }
    FreeRTOS::Addons::LockGuard lock_guard(this->mutex);
    if (!this->ll_ensure_write_readiness(timeout)) {
        return false;
    }
    if (!this->ll_write_async(data, size)) {
        return false;
    }
    if (this->write_semaphore.take(timeout.left()) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::write_async(const std::uint8_t *data, std::size_t size, const Timeout &timeout) {
    if (size == 0) {
        return true;
    }
    if (!data) {
        return false;
    }
    FreeRTOS::Addons::LockGuard lock_guard(this->mutex);
    if (!this->ll_ensure_write_readiness(timeout)) {
        return false;
    }
    if (!this->ll_write_async(data, size)) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::write_await(const Timeout &timeout) {
    FreeRTOS::Addons::LockGuard lock_guard(this->mutex);
    if (!this->ll_busy_writing()) {
        return true;
    }
    if (this->write_semaphore.take(timeout.left()) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::read_write(std::uint8_t *rd_data, const std::uint8_t *wr_data, std::size_t size,
                                const Timeout &timeout) {
    if (size == 0) {
        return true;
    }
    if (!rd_data) {
        return false;
    }
    if (!wr_data) {
        return false;
    }
    FreeRTOS::Addons::LockGuard lock_guard(this->mutex);
    if (!this->ll_ensure_read_readiness(timeout)) {
        return false;
    }
    if (!this->ll_ensure_write_readiness(timeout)) {
        return false;
    }
    if (!this->ll_read_write_async(rd_data, wr_data, size)) {
        return false;
    }
    if (this->read_semaphore.take(timeout.left()) != pdTRUE) {
        return false;
    }
    if (this->write_semaphore.take(timeout.left()) != pdTRUE) {
        return false;
    }
    return true;
}

bool EventDrivenSpi::deinit() { return this->ll_deinit(); }

void EventDrivenSpi::ll_async_complete_common_signal() {
    bool higherPriorityTaskWoken = pdFALSE;
    const auto is_inside_interrupt = xPortIsInsideInterrupt();
    if (is_inside_interrupt) {
        this->write_semaphore.giveFromISR(higherPriorityTaskWoken);
    }
    else {
        this->write_semaphore.give();
    }
    if (is_inside_interrupt) {
        FreeRTOS::Kernel::yieldFromISR(higherPriorityTaskWoken);
    }
    /// @attention FreeRTOS::Kernel::yieldFromISR() must be called last!
}

void EventDrivenSpi::ll_async_read_completed_cb() { this->ll_async_complete_common_signal(); }
void EventDrivenSpi::ll_async_write_completed_cb() { this->ll_async_complete_common_signal(); }
void EventDrivenSpi::ll_async_read_write_completed_cb() { this->ll_async_complete_common_signal(); }
void EventDrivenSpi::ll_async_abnormal_cb() { this->ll_async_complete_common_signal(); }
} // namespace ln::drivers
