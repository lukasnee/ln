// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/ln.h"
#include "ln/interfaces/Semaphore.hpp"
#include "ln/port/ticks.hpp"

#include <FreeRTOS/Semaphore.hpp>
#include <FreeRTOS/Kernel.hpp>

namespace ln {

class BinarySemaphore : public interfaces::BinarySemaphore {
protected:
    bool ll_give() final {
        if (interrupt_context()) {
            bool higher_priority_task_woken = false;
            const bool ok =
                this->semaphore.giveFromISR(higher_priority_task_woken);
            // must be called last in this function scope.
            FreeRTOS::Kernel::yieldFromISR(higher_priority_task_woken);
            return ok;
        }
        return this->semaphore.give();
    }

    bool ll_take(const std::chrono::milliseconds &timeout) final {
        return this->semaphore.take(port::to_ticks(timeout));
    }

    FreeRTOS::StaticBinarySemaphore semaphore;
};

} // namespace ln
