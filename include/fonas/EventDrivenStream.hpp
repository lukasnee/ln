/*
 * fonas - C++ FreeRTOS Framework.
 * Copyright (C) 2023 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "fonas/fonas.hpp"

#include <cstdint>
#include <type_traits>

namespace fonas::EventDriven {

enum StreamType {
    r,
    w,
    rw
};

class ReadStream {
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
    bool read(std::uint8_t *data, std::size_t size, TickType_t timeout_ticks = portMAX_DELAY) {
        if (size == 0) {
            return true;
        }
        if (!data) {
            return false;
        }
        LockGuard lock_guard(this->mutex);
        // assure that the binary semaphore is not already given from previous timed out read() call.
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

    /**
     * @brief Low-level callback signaling read completion (either ISR or thread context).
     */
    void ll_async_read_completed_cb() {
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
    MutexStandard mutex;
    BinarySemaphore semaphore;
};

class WriteStream {

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
    bool write(const std::uint8_t *data, std::size_t size, TickType_t timeout_ticks = portMAX_DELAY) {
        if (size == 0) {
            return true;
        }
        if (!data) {
            return false;
        }
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

    /**
     * @brief Low-level callback signaling write completion (either ISR or thread context).
     */
    void ll_async_write_completed_cb() {
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
    MutexStandard mutex;
    BinarySemaphore semaphore;
};

template <StreamType stream_type> struct StreamBase {};
template <> struct StreamBase<StreamType::r> : public ReadStream {};
template <> struct StreamBase<StreamType::w> : public WriteStream {};
template <> struct StreamBase<StreamType::rw> : public ReadStream, public WriteStream {};

template <StreamType stream_type> class Stream : public StreamBase<stream_type> {

public:
    /**
     * @brief Initialize.
     *
     * @return true success.
     * @return false failure.
     */
    bool init() { return this->ll_init(); }

    /**
     * @brief Deinitialize.
     *
     * @return true success.
     * @return false failure.
     */
    bool deinit() { return this->ll_deinit(); }

protected:
    /**
     * @brief Low-level initialization.
     *
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_init() = 0;

    /**
     * @brief Low-level deinitialization.
     *
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_deinit() = 0;
};

} // namespace fonas::EventDriven
