// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/ln.h"

#include "ln/log.h"

#include <utility>

namespace ln {

void panic_port(const char *file, int line, const char *message);

[[noreturn]] void panic(const char *file, int line, const char *message) {
    if (!ln::interrupt_context()) {
        LN_LOG_FLUSH();
    }
    ln::panic_port(file, line, message);
    ln::sleep(std::chrono::seconds(5));
    ln::reset();
    std::unreachable();
}
} // namespace ln

extern "C" void ln_panic(const char *file, int line, const char *message) {
    ln::panic(file, line, message);
}

extern "C" void ln_reset() { ln::reset(); }
