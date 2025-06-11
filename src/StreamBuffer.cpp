/*
 * fonas - C++ FreeRTOS Framework.
 * Copyright (C) 2023 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "fonas/StreamBuffer.hpp"

namespace fonas {

StreamBuffer::StreamBuffer(std::size_t buffer_size, std::size_t trigger_level) {
    this->handle = xStreamBufferCreate(buffer_size, trigger_level);
    if (!this->handle) {
        configASSERT(!"xStreamBufferCreate() failed");
    }
}

StreamBuffer::~StreamBuffer() {
    if (this->handle) {
        vStreamBufferDelete(this->handle);
    }
}

std::size_t StreamBuffer::send(const std::uint8_t *data, std::size_t size, TickType_t timeout_ticks) {
    if (size == 0) {
        return 0;
    }
    if (!data) {
        return 0;
    }
    if (!this->handle) {
        return 0;
    }
    return xStreamBufferSend(this->handle, reinterpret_cast<const void *>(data), size, timeout_ticks);
}

std::size_t StreamBuffer::send_from_isr(const std::uint8_t *data, std::size_t size,
                                        BaseType_t *const higher_priority_task_woken) {
    if (size == 0) {
        return 0;
    }
    if (!data) {
        return 0;
    }
    if (!this->handle) {
        return 0;
    }
    return xStreamBufferSendFromISR(this->handle, reinterpret_cast<const void *>(data), size,
                                    higher_priority_task_woken);
}

std::size_t StreamBuffer::receive(std::uint8_t *data, std::size_t size, TickType_t timeout_ticks) {
    if (size == 0) {
        return 0;
    }
    if (!data) {
        return 0;
    }
    if (!this->handle) {
        return 0;
    }
    return xStreamBufferReceive(this->handle, reinterpret_cast<void *>(data), size, timeout_ticks);
}

std::size_t StreamBuffer::receive_from_isr(std::uint8_t *data, std::size_t size,
                                           BaseType_t *const higher_priority_task_woken) {
    if (size == 0) {
        return 0;
    }
    if (!data) {
        return 0;
    }
    if (!this->handle) {
        return 0;
    }
    return xStreamBufferReceiveFromISR(this->handle, reinterpret_cast<void *>(data), size, higher_priority_task_woken);
}

bool StreamBuffer::is_full() const {
    if (!this->handle) {
        return false;
    }
    return xStreamBufferIsFull(this->handle) == pdTRUE ? true : false;
}

bool StreamBuffer::is_empty() const {
    if (!this->handle) {
        return false;
    }
    return xStreamBufferIsEmpty(this->handle) == pdTRUE ? true : false;
}

bool StreamBuffer::reset() {
    if (!this->handle) {
        return false;
    }
    return xStreamBufferReset(this->handle) == pdTRUE ? true : false;
}

std::size_t StreamBuffer::spaces_available() const {
    if (!this->handle) {
        return 0;
    }
    return xStreamBufferSpacesAvailable(this->handle);
}

std::size_t StreamBuffer::bytes_available() const {
    if (!this->handle) {
        return 0;
    }
    return xStreamBufferBytesAvailable(this->handle);
}

bool StreamBuffer::set_trigger_level(std::size_t trigger_level) {
    if (!this->handle) {
        return false;
    }
    return xStreamBufferSetTriggerLevel(this->handle, trigger_level) == pdTRUE ? true : false;
}

} // namespace fonas
