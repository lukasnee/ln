// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/interfaces/Mutex.hpp"

#include <chrono>
#include <mutex>

namespace ln {

class Mutex : public interfaces::Mutex {
protected:
    bool ll_lock(const ::std::chrono::milliseconds &timeout) final {
        return this->mutex.try_lock_for(timeout);
    }
    bool ll_unlock() final {
        this->mutex.unlock();
        return true;
    }

    std::timed_mutex mutex;
};

class RecursiveMutex : public interfaces::Mutex {
protected:
    bool ll_lock(const ::std::chrono::milliseconds &timeout) final {
        return this->recursive_mutex.try_lock_for(timeout);
    }
    bool ll_unlock() final {
        this->recursive_mutex.unlock();
        return true;
    }

    std::recursive_timed_mutex recursive_mutex;
};

} // namespace ln
