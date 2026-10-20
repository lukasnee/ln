// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/Clock.hpp"

#include <chrono>

namespace ln {

/**
 * @brief Tracks time elapsed since creation, which helps handling a timeout
 * across a sequence of blocking operations.
 */
class Timeout {
public:
    static Clock::duration max() { return Clock::duration::max(); }

    Timeout() : limit{max()}, initial_time_point{Clock::now()} {}

    template <typename Rep, typename Period>
    Timeout(const std::chrono::duration<Rep, Period> &duration)
        : limit{std::chrono::duration_cast<Clock::duration>(duration)},
          initial_time_point{Clock::now()} {}

    Clock::duration elapsed() const {
        return Clock::now() - initial_time_point;
    }

    bool is_expired() const { return this->elapsed() >= this->limit; }

    /**
     * @brief Remaining time, or zero if already expired.
     */
    Clock::duration left() const {
        const auto elapsed = this->elapsed();
        if (elapsed >= this->limit) {
            return Clock::duration::zero();
        }
        return this->limit - elapsed;
    }

private:
    Clock::duration limit;
    Clock::time_point initial_time_point;
};

} // namespace ln
