// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/ln.h"
#include "ln/interfaces/Queue.hpp"
#include "ln/port/ticks.hpp"

#include <FreeRTOS/Queue.hpp>
#include <FreeRTOS/Kernel.hpp>

namespace ln {

template <typename T, std::size_t N> class Queue : public interfaces::Queue<T> {
protected:
    bool ll_send(const T &item,
                 const std::chrono::milliseconds &timeout) final {
        if (interrupt_context()) {
            bool higher_priority_task_woken = false;
            const bool ok =
                this->queue.sendToBackFromISR(higher_priority_task_woken, item);
            // must be called last in this function scope.
            FreeRTOS::Kernel::yieldFromISR(higher_priority_task_woken);
            return ok;
        }
        return this->queue.sendToBack(item, port::to_ticks(timeout));
    }

    std::optional<T> ll_receive(
        const std::chrono::milliseconds &timeout) final {
        if (interrupt_context()) {
            bool higher_priority_task_woken = false;
            auto item = this->queue.receiveFromISR(higher_priority_task_woken);
            // must be called last in this function scope.
            FreeRTOS::Kernel::yieldFromISR(higher_priority_task_woken);
            return item;
        }
        return this->queue.receive(port::to_ticks(timeout));
    }

    FreeRTOS::StaticQueue<T, N> queue;
};

} // namespace ln
