// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <chrono>

namespace ln {

namespace interfaces {

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

} // namespace interfaces

class Mutex; // Port specific implementation of interfaces::Mutex

class RecursiveMutex; // Port specific implementation of interfaces::Mutex

class LockGuard {
public:
    explicit LockGuard(interfaces::Mutex &mutex) : mutex(mutex) {
        this->mutex.lock();
    }

    ~LockGuard() { this->mutex.unlock(); }

private:
    interfaces::Mutex &mutex;
};

} // namespace ln
