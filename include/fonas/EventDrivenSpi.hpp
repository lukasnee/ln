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

namespace fonas {

class EventDrivenSpi {
public:
    /**
     * @brief Initialize.
     *
     * @return true success.
     * @return false failure.
     */
    bool init();

    /**
     * @brief Read synchronously.
     *
     * @param data
     * @param size
     * @param timeout_ticks
     * @return true success.
     * @return false failure.
     */
    bool read(std::uint8_t *data, std::size_t size, TickType_t timeout_ticks = portMAX_DELAY);

    /**
     * @brief Write synchronously.
     *
     * @param data
     * @param size
     * @param timeout_ticks
     * @return true success.
     * @return false failure.
     */
    bool write(const std::uint8_t *data, std::size_t size, TickType_t timeout_ticks = portMAX_DELAY);

    /**
     * @brief Read-write (full-duplex) synchronously.
     *
     * @param rd_data
     * @param wr_data
     * @param size
     * @param timeout_ticks
     * @return true success.
     * @return false failure.
     */
    bool read_write(std::uint8_t *rd_data, const std::uint8_t *wr_data, std::size_t size,
                    TickType_t timeout_ticks = portMAX_DELAY);

    /**
     * @brief Deinitialize.
     *
     * @return true success.
     * @return false failure.
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

protected:
    /**
     * @brief Low-level initialization.
     *
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_init() = 0;

    /**
     * @brief Low-level asynchronous read.
     *
     * @param data
     * @param size
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_read_async(std::uint8_t *data, std::size_t size) = 0;

    /**
     * @brief Low-level asynchronous write.
     *
     * @param data
     * @param size
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_write_async(const std::uint8_t *data, std::size_t size) = 0;

    /**
     * @brief Low-level asynchronous read-write (full-duplex).
     *
     * @param rd_data
     * @param wr_data
     * @param size
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_read_write_async(std::uint8_t *rd_data, const std::uint8_t *wr_data, std::size_t size) = 0;

    /**
     * @brief Low-level deinitialization.
     *
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_deinit() = 0;

private:
    MutexStandard mutex;
    BinarySemaphore semaphore;
};

} // namespace fonas
