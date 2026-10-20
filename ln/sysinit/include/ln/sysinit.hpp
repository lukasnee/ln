// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/algo/CompileTimeDAG.hpp"

#include "ln/log.h"

namespace ln::sysinit {

LN_LOG_MODULE(sysinit, ln::logger::Level::notset);

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
        LN_LOG(result ? ln::logger::Level::info : ln::logger::Level::critical,
               "{:>2}/{} {}: {}", index + 1, ordered_initializers.size(),
               (result ? "OK" : "FAIL"), initializer.name);
        if (!result) {
            LN_PANIC();
        }
        ++index;
    }
}

#define LN_SYSINIT_ID_ENUMS_OP(id, fn) id,
#define LN_SYSINIT_ARRAY_ELEMENT_OP(id, fn) ln::sysinit::Unit{ID::id, #id, fn},
#define LN_SYSINIT_DEPS_OP(id_from, id_to)                                     \
    ln::sysinit::Dep{ID::id_from, ID::id_to},

#define LN_SYSINIT_DECLARE(_name, LN_SYSINIT_UNITS, LN_SYSINIT_DEPS)           \
    namespace _name {                                                          \
    enum class ID {                                                            \
        LN_SYSINIT_UNITS(LN_SYSINIT_ID_ENUMS_OP)                               \
    };                                                                         \
    constexpr std::array items_unsorted = {                                    \
        LN_SYSINIT_UNITS(LN_SYSINIT_ARRAY_ELEMENT_OP)};                        \
    constexpr std::array deps = {LN_SYSINIT_DEPS(LN_SYSINIT_DEPS_OP)};         \
    static constexpr auto items_sorted =                                       \
        ln::algo::CompileTimeDAG::Graph(items_unsorted, deps).topo_sort();     \
    }

#define LN_SYSINIT_INIT(_name) ln::sysinit::init(_name::items_sorted);

} // namespace ln::sysinit
