/*
 * Copyright (C) 2023 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "FreeRTOS/Task.hpp"
#include "FreeRTOS/Queue.hpp"

#include <cstdint>

namespace ln::drivers {

class QueueWriter : public FreeRTOS::Task {
public:
    QueueWriter(UBaseType_t size, const char *name, configSTACK_DEPTH_TYPE stack_depth, UBaseType_t priority)
        : FreeRTOS::Task(priority, stack_depth, name), queue(size) {}

    bool write_in(const uint8_t &byte) { return this->queue.sendToBack(byte, portMAX_DELAY); }

protected:
    virtual void write_out(const uint8_t &byte) = 0;

private:
    void taskFunction() final {
        while (true) {
            auto opt_byte = this->queue.receive(portMAX_DELAY);
            if (opt_byte) {
                this->write_out(*opt_byte);
            }
        }
    }

    FreeRTOS::Queue<uint8_t> queue;
};

} // namespace ln::drivers
