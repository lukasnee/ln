// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/ln.h"

#include "ln/Clock.hpp"
#include "ln/MutexI.hpp"

#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"

#include "FreeRTOS.h"
#include "task.h"

namespace ln {

/**
 * @brief Converts a TickType_t to time_point.
 *
 * @note Expressed in similar style as
 * <https://en.cppreference.com/w/cpp/chrono/system_clock/from_time_t.html>.
 *
 * @param t TickType_t value to convert.
 * @return time_point The time_point corresponding to the TickType_t.
 */
static constexpr Clock::time_point from_tick_count(TickType_t t) noexcept {
    return Clock::time_point{std::chrono::milliseconds(
        t * (static_cast<TickType_t>(std::milli::den)) / configTICK_RATE_HZ)};
}

Clock::time_point Clock::now() noexcept {
    return from_tick_count(interrupt_context()
                               ? FreeRTOS::Kernel::getTickCount()
                               : FreeRTOS::Kernel::getTickCountFromISR());
}

static_assert(Clock::duration::max().count() == portMAX_DELAY,
              "Clock::duration::max() must be equivalent "
              "to portMAX_DELAY");

std::chrono::milliseconds MutexI::max_timeout() {
    return Clock::duration::max();
}

std::chrono::milliseconds get_uptime_ms() {
    return duration_cast<std::chrono::milliseconds>(
        Clock::now().time_since_epoch());
}

void sleep(std::chrono::milliseconds duration) {
    if (duration >= Clock::duration::max()) {
        vTaskDelay(portMAX_DELAY);
    }
    else {
        vTaskDelay(
            static_cast<TickType_t>(duration.count() * configTICK_RATE_HZ /
                                    static_cast<TickType_t>(std::milli::den)));
    }
}

void sleep_until(ln::Clock::time_point time_point) {
    const auto now = Clock::now();
    if (time_point > now) {
        sleep(std::chrono::duration_cast<std::chrono::milliseconds>(time_point -
                                                                    now));
    }
}

bool interrupt_context() { return xPortIsInsideInterrupt(); }

std::string_view get_task_name() {
    return pcTaskGetName(xTaskGetCurrentTaskHandle());
}

static BaseType_t taskPrioToFreeRTOSPrio(Task::Prio prio) {
    switch (prio) {
    case Task::Prio::lowest:
        return 0;
    case Task::Prio::low:
        return 1;
    case Task::Prio::low2:
        return 2;
    case Task::Prio::medium:
        return 3;
    case Task::Prio::medium2:
        return 4;
    case Task::Prio::high:
        return 5;
    case Task::Prio::high2:
        return 6;
    case Task::Prio::highest:
        return 7;
    default:
        return 0;
    }
}

Task::Task(const Cfg &cfg) : cfg(cfg) {

    if (cfg.start_on_creation) {
        this->start();
    }
}

bool Task::start() {
    auto taskCode = [](void *param) {
        while (true) {
            static_cast<Task *>(param)->routine();
        }
    };
    if (!this->cfg.static_stack.empty()) {
        if (this->cfg.static_stack.size() <= sizeof(StaticTask_t)) {
            return false;
        }
        auto stackDepth = this->cfg.static_stack.size() - sizeof(StaticTask_t);
        StaticTask_t *taskBuffer = reinterpret_cast<StaticTask_t *>(
            this->cfg.static_stack.data() + this->cfg.static_stack.size() -
            sizeof(StaticTask_t));
        const auto task_handle = xTaskCreateStatic(
            taskCode, this->cfg.name, stackDepth / sizeof(StackType_t), this,
            taskPrioToFreeRTOSPrio(this->cfg.prio),
            reinterpret_cast<StackType_t *>(this->cfg.static_stack.data()),
            taskBuffer);
        if (!task_handle) {
            return false;
        }
        return true;
    }
    if (this->cfg.dynamic_stack_size > 0) {
        if (pdPASS !=
            xTaskCreate(taskCode, this->cfg.name,
                        this->cfg.dynamic_stack_size / sizeof(StackType_t),
                        this, taskPrioToFreeRTOSPrio(this->cfg.prio),
                        nullptr)) {
            return false;
        }
        return true;
    }
    return false;
}

static_assert(Task::Prio::_enum_size <= configMAX_PRIORITIES,
              "invalid priority configration!");

/** @brief FreeRTOS memory allocation lua_Alloc type function for Lua VM. Usage
 * example, `lua_newstate(l_alloc_freertos, nullptr, 0)`.
 */
void *l_alloc_freertos(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud;    /* Not used in this simple example */
    (void)osize; /* Not used in FreeRTOS heap_4/5 */

    if (nsize == 0) {
        vPortFree(ptr);
        return NULL;
    }
    /* FreeRTOS pvPortMalloc does not have a native 'realloc'.
       We must handle the re-allocation manually. */
    if (!ptr) {
        return pvPortMalloc(nsize);
    }
    // Check if we are shrinking or growing
    void *newptr = pvPortMalloc(nsize);
    if (newptr) {
        // Only copy the minimum of old and new size
        size_t copy_size = (osize < nsize) ? osize : nsize;
        memcpy(newptr, ptr, copy_size);
        vPortFree(ptr);
    }
    return newptr;
}

} // namespace ln

extern "C" void vApplicationMallocFailedHook(void) { LN_PANIC(); }
