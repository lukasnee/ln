/*
 * Copyright (C) 2023 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "ln/ln.hpp"
#include "ln/drivers/Initializable.hpp"

#include "FreeRTOS/Semaphore.hpp"
#include "FreeRTOS/Addons/LockGuard.hpp"
#include "FreeRTOS/Addons/Timeout.hpp"
#include "FreeRTOS/Addons/Kernel.hpp"

#include <cstdint>
#include <type_traits>

namespace ln::drivers::EventDriven {

enum StreamType {
    r,
    w,
    rw
};

class ReadStream : public virtual Initializable {
public:
    /**
     * @brief Read synchronously.
     *
     * @param data
     * @param size
     * @param timeout_ticks
     * @return true success.
     * @return false failure.
     */
    bool read(std::uint8_t *data, std::size_t size, FreeRTOS::Addons::Timeout timeout = {}) {
        if (size == 0) {
            return true;
        }
        if (!data) {
            return false;
        }
        FreeRTOS::Addons::LockGuard lock_guard(this->mutex);
        if (!this->initialized) {
            return false;
        }
        // assure that the binary semaphore is not already given from previous timed out read() call.
        // TODO: check if this is necessary.
        this->semaphore.give();
        this->semaphore.take();

        if (!this->ll_read_async(data, size)) {
            return false;
        }
        if (this->semaphore.take(timeout.left().count()) != pdTRUE) {
            return false;
        }
        return true;
    }

    /**
     * @brief Low-level callback signaling read completion (either ISR or thread context).
     */
    void ll_async_read_completed_cb() {
        bool higherPriorityTaskWoken = false;
        const auto is_inside_interrupt = FreeRTOS::Addons::Kernel::isInsideInterrupt();
        if (is_inside_interrupt) {
            this->semaphore.giveFromISR(higherPriorityTaskWoken);
        }
        else {
            this->semaphore.give();
        }

        if (is_inside_interrupt) {
            // must be called last in this function scope.
            FreeRTOS::Kernel::yieldFromISR(higherPriorityTaskWoken);
        }
    }

protected:
    /**
     * @brief Low-level asynchronous read.
     *
     * @param data
     * @param size
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_read_async(std::uint8_t *data, std::size_t size) = 0;

public:
    FreeRTOS::StaticMutex mutex;
    FreeRTOS::BinarySemaphore semaphore;
};

class WriteStream : public virtual Initializable {

public:
    /**
     * @brief Write synchronously.
     *
     * @param data
     * @param size
     * @param timeout_ticks
     * @return true success.
     * @return false failure.
     */
    bool write(const std::uint8_t *data, std::size_t size, FreeRTOS::Addons::Timeout timeout = {}) {
        if (size == 0) {
            return true;
        }
        if (!data) {
            return false;
        }
        FreeRTOS::Addons::LockGuard lock_guard(this->mutex);
        if (!this->initialized) {
            return false;
        }
        // assure that the binary semaphore is not already given from previous timed out write() call.
        // TODO: check if this is necessary.
        this->semaphore.give();
        this->semaphore.take();

        if (!this->ll_write_async(data, size)) {
            return false;
        }
        if (this->semaphore.take(timeout.left().count()) != pdTRUE) {
            return false;
        }
        return true;
    }

    /**
     * @brief Low-level callback signaling write completion (either ISR or thread context).
     */
    void ll_async_write_completed_cb() {
        bool higherPriorityTaskWoken = false;
        const auto is_inside_interrupt = FreeRTOS::Addons::Kernel::isInsideInterrupt();
        if (is_inside_interrupt) {
            this->semaphore.giveFromISR(higherPriorityTaskWoken);
        }
        else {
            this->semaphore.give();
        }

        if (is_inside_interrupt) {
            // must be called last in this function scope.
            FreeRTOS::Kernel::yieldFromISR(higherPriorityTaskWoken);
        }
    }

protected:
    /**
     * @brief Low-level asynchronous write.
     *
     * @param data
     * @param size
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_write_async(const std::uint8_t *data, std::size_t size) = 0;

private:
    FreeRTOS::StaticMutex mutex;
    FreeRTOS::BinarySemaphore semaphore;
};

template <StreamType stream_type> struct Stream {};
template <> struct Stream<StreamType::r> : public ReadStream {};
template <> struct Stream<StreamType::w> : public WriteStream {};
template <> struct Stream<StreamType::rw> : public ReadStream, public WriteStream {};

} // namespace ln::drivers::EventDriven
