// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/interfaces/Thread.hpp"

#include "FreeRTOS.h"
#include "task.h"

namespace ln {

class Thread : public interfaces::Thread {
public:
    explicit Thread(const Cfg &cfg) : interfaces::Thread(cfg) {
        if (cfg.start_on_creation) {
            this->start();
        }
    }

    bool start() final;

protected:
    TaskHandle_t task_handle = nullptr;
};

} // namespace ln
