// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ln {

namespace interfaces {

class Thread {
public:
    enum Prio {
        lowest,
        low,
        low2,
        medium,
        medium2,
        high,
        high2,
        highest,

        _enum_size
    };

    struct Cfg {
        Prio prio;
        const char *name;
        std::span<std::uint8_t> static_stack = {};
        std::size_t dynamic_stack_size = 0;
        bool start_on_creation = false;
    };

    explicit Thread(const Cfg &cfg) : cfg(cfg) {}
    virtual ~Thread() = default;

    virtual bool start() = 0;

    virtual void routine() = 0;

protected:
    const Cfg &cfg;
};

} // namespace interfaces

class Thread; // Port specific implementation of interfaces::Thread

} // namespace ln
