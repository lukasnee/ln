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

#include <cstdint>

namespace ln::drivers {

class EventDrivenSpi {
public:
    /**
     * @brief Initialize.
     *
     * @return true if successful, otherwise false.
     */
    bool init();

    /**
     * @brief Read synchronously.
     *
     * @param data
     * @param size
     * @param timeout
     * @return true if successful, otherwise false.
     */
    bool read(std::uint8_t *data, std::size_t size, ln::Timeout timeout = ln::Timeout(portMAX_DELAY));

    /**
     * @brief Write synchronously.
     *
     * @param data
     * @param size
     * @param timeout
     * @return true if successful, otherwise false.
     */
    bool write(const std::uint8_t *data, std::size_t size, ln::Timeout timeout = ln::Timeout(portMAX_DELAY));

    /**
     * @brief Write asynchronously.
     *
     * @param data
     * @param size
     * @param timeout
     * @return true if successful, otherwise false.
     */
    bool write_async(const std::uint8_t *data, std::size_t size, ln::Timeout timeout = ln::Timeout(portMAX_DELAY));

    /**
     * @brief Await write_async completion.
     *
     * @param timeout
     * @return true if successful, otherwise false.
     */
    bool write_await(ln::Timeout timeout = ln::Timeout(portMAX_DELAY));

    /**
     * @brief Read-write (full-duplex) synchronously.
     *
     * @param rd_data
     * @param wr_data
     * @param size
     * @param timeout
     * @return true if successful, otherwise false.
     */
    bool read_write(std::uint8_t *rd_data, const std::uint8_t *wr_data, std::size_t size,
                    ln::Timeout timeout = ln::Timeout(portMAX_DELAY));

    /**
     * @brief Deinitialize.
     *
     * @return true if successful, otherwise false.
     */
    bool deinit();

    /**
     * @brief Low-level callback signaling read completion (either ISR or thread context).
     */
    void ll_async_read_completed_cb();

    /**
     * @brief Low-level callback signaling write completion (either ISR or thread context).
     */
    void ll_async_write_completed_cb();

    /**
     * @brief Low-level callback signaling read-write (full-duplex) completion. (either ISR or thread context).
     */
    void ll_async_read_write_completed_cb();

    /**
     * @brief Low-level callback signaling abnormal completion (error, abort, suspend) (either ISR or thread context).
     */
    void ll_async_abnormal_cb();

protected:
    /**
     * @brief Low-level initialization.
     *
     * @return true if successful, otherwise false.
     */
    virtual bool ll_init() = 0;

    /**
     * @brief Low-level asynchronous read.
     *
     * @param data
     * @param size
     * @return true if successful, otherwise false.
     */
    virtual bool ll_read_async(std::uint8_t *data, std::size_t size) = 0;

    /**
     * @brief Low-level asynchronous write.
     *
     * @param data
     * @param size
     * @return true if successful, otherwise false.
     */
    virtual bool ll_write_async(const std::uint8_t *data, std::size_t size) = 0;

    /**
     * @brief Low-level asynchronous read-write (full-duplex).
     *
     * @param rd_data
     * @param wr_data
     * @param size
     * @return true if successful, otherwise false.
     */
    virtual bool ll_read_write_async(std::uint8_t *rd_data, const std::uint8_t *wr_data, std::size_t size) = 0;

    /**
     * @brief Check if low-level driver is busy writing. For example, DMA Tx is
     * in progress.
     *
     * @return true if busy, otherwise failure.
     */
    virtual bool ll_busy_writing() = 0;

    /**
     * @brief Check if low-level driver is busy reading. For example, DMA Rx is
     * in progress.
     *
     * @return true if busy, otherwise failure.
     */
    virtual bool ll_busy_reading() = 0;

    /**
     * @brief Low-level deinitialization.
     *
     * @return true if successful, otherwise false.
     */
    virtual bool ll_deinit() = 0;

private:
    /**
     * @brief Ensure low-level driver readiness for reading.
     *
     * @param timeout
     * @return true if successful, otherwise false.
     */
    bool ll_ensure_read_readiness(ln::Timeout timeout);

    /**
     * @brief Ensure low-level driver readiness for writing.
     *
     * @param timeout
     * @return true if successful, otherwise false.
     */
    bool ll_ensure_write_readiness(ln::Timeout timeout);

    void ll_async_complete_common_signal();

    MutexStandard mutex;
    BinarySemaphore write_semaphore;
    BinarySemaphore read_semaphore;
};

} // namespace ln::drivers
