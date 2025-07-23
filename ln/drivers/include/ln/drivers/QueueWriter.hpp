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

class QueueWriter {
public:
    class thread : public ::thread {
    public:
        thread(QueueWriter &queue_writer, const char *name, uint16_t stack_depth, UBaseType_t priority)
            : ::thread(name, stack_depth, priority) {}

        void run() final {
            while (true) {
                uint8_t byte;
                if (this->queue_writer.queue.Dequeue(&byte, portMAX_DELAY)) {
                    this->queue_writer.write_out(byte);
                }
            }
        }

    private:
        QueueWriter &queue_writer;
    }

    QueueWriter(UBaseType_t size, const char *name, uint16_t stack_depth, UBaseType_t priority)
        : queue(size, 1), thread(queue, name, stack_depth, priority) {
    }

    void init() { this->thread.start(); }

    bool write_in(const uint8_t &byte) { return this->queue.Enqueue(&byte, portMAX_DELAY); }

protected:
    virtual void write_out(const uint8_t &byte) = 0;

private:
    friend class thread;

    queue queue;
    thread thread;
};

} // namespace ln::drivers
