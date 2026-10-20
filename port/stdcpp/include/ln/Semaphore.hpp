// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/interfaces/Semaphore.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace ln {

class BinarySemaphore : public interfaces::BinarySemaphore {
protected:
    bool ll_give() final {
        {
            const std::lock_guard lock(this->mutex);
            this->given = true;
        }
        this->cv.notify_one();
        return true;
    }

    bool ll_take(const std::chrono::milliseconds &timeout) final {
        std::unique_lock lock(this->mutex);
        const auto is_given = [this] { return this->given; };
        if (timeout >= Clock::duration::max()) {
            this->cv.wait(lock, is_given);
        }
        else if (!this->cv.wait_for(lock, timeout, is_given)) {
            return false;
        }
        this->given = false;
        return true;
    }

    std::mutex mutex;
    std::condition_variable cv;
    bool given = false;
};

} // namespace ln
