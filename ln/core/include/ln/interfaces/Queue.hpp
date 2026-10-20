// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/Clock.hpp"

#include <chrono>
#include <cstddef>
#include <optional>

namespace ln {

namespace interfaces {

template <typename T> class Queue {
public:
    virtual ~Queue() = default;

    /**
     * @brief Send an item to the back of the queue. Safe to call from interrupt
     * context, where the timeout is ignored.
     *
     * @retval true if sent, false if the queue stayed full.
     */
    bool send(const T &item) {
        return this->ll_send(item, Clock::duration::max());
    }

    template <typename Rep, typename Period>
    bool send(const T &item,
              const std::chrono::duration<Rep, Period> &timeout) {
        return this->ll_send(
            item,
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
    }

    /**
     * @brief Receive an item from the front of the queue. Safe to call from
     * interrupt context, where the timeout is ignored.
     */
    std::optional<T> receive() {
        return this->ll_receive(Clock::duration::max());
    }

    template <typename Rep, typename Period>
    std::optional<T> receive(
        const std::chrono::duration<Rep, Period> &timeout) {
        return this->ll_receive(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
    }

protected:
    virtual bool ll_send(const T &item,
                         const std::chrono::milliseconds &timeout) = 0;
    virtual std::optional<T> ll_receive(
        const std::chrono::milliseconds &timeout) = 0;
};

} // namespace interfaces

// Port specific implementation of interfaces::Queue with static storage for N
// items.
template <typename T, std::size_t N> class Queue;

} // namespace ln
