// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/ln.h"

#ifdef LN_LOGGER
#include "ln/logger/logger.h"
#endif

#include <utility>

namespace ln {

void panic_port(const char *file, int line, const char *message);

[[noreturn]] void panic(const char *file, int line, const char *message) {
#ifdef LN_LOGGER
    if (!ln::interrupt_context()) {
        ln::logger::get_instance().flush_buffer();
    }
#endif
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
