// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/Clock.hpp"

#include <chrono>

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

protected:
    virtual bool ll_give() = 0;
    virtual bool ll_take(const std::chrono::milliseconds &timeout) = 0;
};

} // namespace interfaces

class BinarySemaphore; // Port specific implementation of
                       // interfaces::BinarySemaphore

} // namespace ln
