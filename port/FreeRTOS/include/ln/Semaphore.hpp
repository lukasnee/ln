// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/interfaces/Semaphore.hpp"
#include "ln/port/ticks.hpp"

#include <FreeRTOS/Semaphore.hpp>

namespace ln {

class BinarySemaphore : public interfaces::BinarySemaphore {
protected:
    bool ll_give() final { return this->semaphore.give(); }

    std::tuple<bool, bool> ll_give_from_isr() final {
        bool higher_priority_task_woken = false;
        return {this->semaphore.giveFromISR(higher_priority_task_woken),
                higher_priority_task_woken};
    }

    bool ll_take(const std::chrono::milliseconds &timeout) final {
        return this->semaphore.take(port::to_ticks(timeout));
    }

    std::tuple<bool, bool> ll_take_from_isr() final {
        bool higher_priority_task_woken = false;
        return {this->semaphore.takeFromISR(higher_priority_task_woken),
                higher_priority_task_woken};
    }

    FreeRTOS::StaticBinarySemaphore semaphore;
};

} // namespace ln
