// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <chrono>

namespace ln {

namespace osal {

class Mutex {
public:
    virtual ~Mutex() = default;

    /**
     * @brief Returns the maximum timeout that can be used in the lock()
     * function.
     *
     * @attention Port specific implementation.
     *
     * @retval std::chrono::milliseconds
     */
    static std::chrono::milliseconds max_timeout();

    bool lock() { return this->ll_lock(max_timeout()); }

    template <typename Rep, typename Period>
    bool lock(const std::chrono::duration<Rep, Period> &timeout) {
        return this->ll_lock(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
    }

    bool unlock() { return this->ll_unlock(); }

protected:
    virtual bool ll_lock(const std::chrono::milliseconds &timeout) = 0;
    virtual bool ll_unlock() = 0;
};

} // namespace osal

class Mutex; // Port specific implementation of osal::Mutex

class RecursiveMutex; // Port specific implementation of osal::Mutex

class LockGuard {
public:
    explicit LockGuard(osal::Mutex &mutex) : mutex(mutex) {
        this->mutex.lock();
    }

    ~LockGuard() { this->mutex.unlock(); }

private:
    osal::Mutex &mutex;
};

} // namespace ln
