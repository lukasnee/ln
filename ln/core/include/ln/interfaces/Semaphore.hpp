// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/Clock.hpp"

#include <chrono>
#include <tuple>

namespace ln {

namespace interfaces {

class BinarySemaphore {
public:
    virtual ~BinarySemaphore() = default;

    /**
     * @brief Give the semaphore. Safe to call from interrupt context.
     *
     * @retval true if successful, otherwise false.
     */
    bool give() { return this->ll_give(); }

    /**
     * @brief Give the semaphore from interrupt context.
     *
     * @result Tuple of (result, higher_priority_thread_woken).
     * ln::yield_from_isr(higher_priority_thread_woken) may be called at the
     * very end of the ISR handling if result is true.
     */
    std::tuple<bool, bool> give_from_isr() { return this->ll_give_from_isr(); }

    bool take() { return this->ll_take(Clock::duration::max()); }

    /**
     * @brief Take the semaphore. Not allowed in interrupt context.
     *
     * @retval true if taken, false on timeout.
     */
    template <typename Rep, typename Period>
    bool take(const std::chrono::duration<Rep, Period> &timeout) {
        return this->ll_take(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
    }

    /**
     * @brief Take the semaphore from interrupt context.
     *
     * @result Tuple of (result, higher_priority_thread_woken).
     * ln::yield_from_isr(higher_priority_thread_woken) may be called at the
     * very end of the ISR handling if result is true.
     */
    std::tuple<bool, bool> take_from_isr() { return this->ll_take_from_isr(); }

protected:
    virtual bool ll_give() = 0;
    virtual std::tuple<bool, bool> ll_give_from_isr() = 0;
    virtual bool ll_take(const std::chrono::milliseconds &timeout) = 0;
    virtual std::tuple<bool, bool> ll_take_from_isr() = 0;
};

} // namespace interfaces

class BinarySemaphore; // Port specific implementation of
                       // interfaces::BinarySemaphore

} // namespace ln
