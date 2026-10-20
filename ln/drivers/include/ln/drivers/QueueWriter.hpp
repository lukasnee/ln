// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/Thread.hpp"
#include "ln/Queue.hpp"

#include <cstddef>
#include <cstdint>

namespace ln::drivers {

template <std::size_t N> class QueueWriter : public ln::Thread {
public:
    explicit QueueWriter(const ln::Thread::Cfg &cfg) : ln::Thread(cfg) {}

    bool write_in(const std::uint8_t &byte) { return this->queue.send(byte); }

protected:
    virtual void write_out(const std::uint8_t &byte) = 0;

private:
    void routine() final {
        const auto opt_byte = this->queue.receive();
        if (opt_byte) {
            this->write_out(*opt_byte);
        }
    }

    Queue<std::uint8_t, N> queue;
};

} // namespace ln::drivers
