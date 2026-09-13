// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/algo/CompileTimeDAG.hpp"

#ifdef LN_LOGGER
#include "ln/logger/logger.h"
#endif

namespace ln::sysinit {

LOG_MODULE(sysinit, ln::logger::Level::notset);

template <typename ID> using Dep = ln::algo::CompileTimeDAG::Edge<ID>;

template <typename ID> struct Unit {
    ID key;
    std::string_view name;
    bool (*init_fn)() = nullptr;
};

template <typename T> static void init(T &ordered_initializers) {
    size_t index = 0;
    for (auto &initializer : ordered_initializers) {
        const auto result = initializer.init_fn ? initializer.init_fn() : true;
#ifdef LN_LOGGER
        LOG(result ? ln::logger::Level::info : ln::logger::Level::critical,
            "{:>2}/{} {}: {}", index + 1, ordered_initializers.size(),
            (result ? "OK" : "FAIL"), initializer.name);
#endif
        if (!result) {
            LN_PANIC();
        }
        ++index;
    }
}

} // namespace ln::sysinit
