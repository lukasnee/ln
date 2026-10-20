// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/interfaces/Mutex.hpp"
#include "projdefs.h"

#include <FreeRTOS/Mutex.hpp>
#include <FreeRTOS/Kernel.hpp>

namespace ln {

class Mutex : public interfaces::Mutex {
protected:
    bool ll_lock(const std::chrono::milliseconds &timeout) final {
        return this->mutex.lock(pdMS_TO_TICKS(timeout.count()));
    }

    bool ll_unlock() final { return this->mutex.unlock(); }

    FreeRTOS::StaticMutex mutex;
};

class RecursiveMutex : public interfaces::Mutex {
protected:
    bool ll_lock(const std::chrono::milliseconds &timeout) final {
        return this->recursive_mutex.lock(pdMS_TO_TICKS(timeout.count()));
    }

    bool ll_unlock() final { return this->recursive_mutex.unlock(); }

    FreeRTOS::StaticRecursiveMutex recursive_mutex;
};

} // namespace ln
