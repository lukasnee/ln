// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/ln.h"
#include "ln/Mutex.hpp"
#include "ln/Semaphore.hpp"
#include "ln/Timeout.hpp"
#include "ln/drivers/Initializable.hpp"

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
     * @param timeout
     * @return true success.
     * @return false failure.
     */
    bool read(std::uint8_t *data, std::size_t size, Timeout timeout = {}) {
        if (size == 0) {
            return true;
        }
        if (!data) {
            return false;
        }
        LockGuard lock_guard(this->mutex);
        if (!this->initialized) {
            return false;
        }
        // assure that the binary semaphore is not already given from previous
        // timed out read() call.
        // TODO: check if this is necessary.
        this->semaphore.give();
        this->semaphore.take();

        if (!this->ll_read_async(data, size)) {
            return false;
        }
        if (!this->semaphore.take(timeout.left())) {
            return false;
        }
        return true;
    }

    /**
     * @brief Low-level callback signaling read completion (either ISR or thread
     * context).
     */
    void ll_async_read_completed_cb() { this->semaphore.give(); }

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
    Mutex mutex;
    BinarySemaphore semaphore;
};

class WriteStream : public virtual Initializable {

public:
    /**
     * @brief Write synchronously.
     *
     * @param data
     * @param size
     * @param timeout
     * @return true success.
     * @return false failure.
     */
    bool write(const std::uint8_t *data, std::size_t size,
               Timeout timeout = {}) {
        if (size == 0) {
            return true;
        }
        if (!data) {
            return false;
        }
        LockGuard lock_guard(this->mutex);
        if (!this->initialized) {
            return false;
        }
        // assure that the binary semaphore is not already given from previous
        // timed out write() call.
        // TODO: check if this is necessary.
        this->semaphore.give();
        this->semaphore.take();

        if (!this->ll_write_async(data, size)) {
            return false;
        }
        if (!this->semaphore.take(timeout.left())) {
            return false;
        }
        return true;
    }

    /**
     * @brief Low-level callback signaling write completion (either ISR or
     * thread context).
     */
    void ll_async_write_completed_cb() { this->semaphore.give(); }

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
    Mutex mutex;
    BinarySemaphore semaphore;
};

template <StreamType stream_type> struct Stream {};
template <> struct Stream<StreamType::r> : public ReadStream {};
template <> struct Stream<StreamType::w> : public WriteStream {};
template <>
struct Stream<StreamType::rw> : public ReadStream, public WriteStream {};

} // namespace ln::drivers::EventDriven
