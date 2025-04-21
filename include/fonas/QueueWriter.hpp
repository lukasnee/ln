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

class QueueWriter {
public:
    class Thread : public ::Thread {
    public:
        Thread(QueueWriter &queueWriter, const char *Name, uint16_t StackDepth, UBaseType_t Priority)
            : ::Thread(Name, StackDepth, Priority) {}

        void run() final {
            while (true) {
                uint8_t byte;
                if (this->queueWriter.queue.Dequeue(&byte, portMAX_DELAY)) {
                    this->queueWriter.writeOut(byte);
                }
            }
        }

    private:
        QueueWriter &queueWriter;
    }

    QueueWriter(UBaseType_t size, const char *Name, uint16_t StackDepth, UBaseType_t Priority)
        : Queue(size, 1), thread(queue, Name, StackDepth, Priority) {
    }

    void init() { this->thread.start(); }

    bool writeIn(const uint8_t &byte) { return this->queue.Enqueue(&byte, portMAX_DELAY); }

protected:
    virtual void writeOut(const uint8_t &byte) = 0;

private:
    friend class Thread;

    Queue queue;
    Thread thread;
};

} // namespace fonas
