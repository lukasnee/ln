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

#include "FreeRTOS.h"
#include "stream_buffer.h"

#include <cstddef>
#include <cstdint>

namespace fonas {

/**
 * @brief Stream Buffer. Stream buffers allow a stream of bytes to be passed from an interrupt service routine to a
 * task, or from one task to another task. A byte stream can be of arbitrary length and does not necessarily have a
 * beginning or end. Any number of bytes can be written in one go, and any number of bytes can be read in one go. Data
 * is passed by copy - the data is copied into the buffer by the sender and out of the buffer by the read.
 *
 * @ref https://www.freertos.org/RTOS-stream-buffer-example.html
 */
class StreamBuffer {
public:
    /**
     * @brief Construct a new Stream Buffer.
     *
     * @param buffer_size
     * @param trigger_level The number of bytes that must be in the stream buffer before a task that is blocked on the
     * stream buffer to wait for data is moved out of the blocked state.
     */
    StreamBuffer(std::size_t buffer_size, std::size_t trigger_level);

    /**
     * @brief Destruct the Stream Buffer.
     *
     */
    ~StreamBuffer();

    /**
     * @brief Send data to the buffer.
     *
     * @param data
     * @param size
     * @param timeout_ticks
     * @return std::size_t
     */
    std::size_t send(const std::uint8_t *data, std::size_t size, TickType_t timeout_ticks = portMAX_DELAY);

    /**
     * @brief Send data to the buffer from ISR context.
     *
     * @param data
     * @param size
     * @param higher_priority_task_woken
     * @return std::size_t
     */
    std::size_t send_from_isr(const std::uint8_t *data, std::size_t size,
                              BaseType_t *const higher_priority_task_woken = nullptr);

    /**
     * @brief Receive data from the buffer.
     *
     * @param data
     * @param size
     * @param timeout_ticks
     * @return std::size_t The number of bytes actually read. If it is less than the size requested, it is known that
     * the read timed out.
     */
    std::size_t receive(std::uint8_t *data, std::size_t size, TickType_t timeout_ticks = portMAX_DELAY);

    /**
     * @brief Receive data from the buffer from ISR context.
     *
     * @param data
     * @param size
     * @param higher_priority_task_woken
     * @return std::size_t
     */
    std::size_t receive_from_isr(std::uint8_t *data, std::size_t size,
                                 BaseType_t *const higher_priority_task_woken = nullptr);

    /**
     * @brief Check if the buffer is full.
     *
     * @retval true
     * @retval false
     */
    bool is_full() const;

    /**
     * @brief Check if the buffer is empty.
     *
     * @retval true
     * @retval false
     */
    bool is_empty() const;

    /**
     * @brief Reset the buffer.
     *
     * @retval true success.
     * @retval false failure.
     */
    bool reset();

    /**
     * @brief Get the number of free space available in the buffer.
     *
     * @return std::size_t
     */
    std::size_t spaces_available() const;

    /**
     * @brief Get the number of bytes the stream buffer contains.
     *
     * @return std::size_t
     */
    std::size_t bytes_available() const;

    /**
     * @brief Set the trigger level - the number of bytes that must be in the stream buffer before a task that is
     * blocked on the stream buffer to wait for data is moved out of the blocked state.
     *
     * @param trigger_level
     *
     * @retval true success.
     * @retval false failure.
     */
    bool set_trigger_level(std::size_t trigger_level);

private:
    StreamBufferHandle_t handle = nullptr;
};

} // namespace fonas