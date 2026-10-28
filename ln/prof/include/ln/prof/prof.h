// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <stdint.h>

typedef struct {
    uint8_t profile_interrupts : 1;
    uint8_t reserved : 7;
} ln_prof_config_t;

#ifndef LN_PROF

#define LN_PROF_START(config)
#define LN_PROF_STOP()
#define LN_PROF_ENTER(id)
#define LN_PROF_EXIT(id)
#define LN_PROF_SCOPE()

#else

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LN_PROF_ATTR __attribute__((optimize("O3"), hot))

    extern "C" LN_PROF_ATTR bool ln_prof_start(ln_prof_config_t config);
#define LN_PROF_START(config) ln_prof_start(config)
    extern "C" LN_PROF_ATTR void ln_prof_stop();
#define LN_PROF_STOP() ln_prof_stop()

    extern "C" LN_PROF_ATTR void ln_prof_scope_enter(void *marker);
#define LN_PROF_ENTER(marker) ln_prof_scope_enter((void *)(marker))

    extern "C" LN_PROF_ATTR void ln_prof_scope_exit(void *marker);
#define LN_PROF_EXIT(marker) ln_prof_scope_exit((void *)(marker))

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace ln::prof {

class Scope {
public:
    __attribute__((no_instrument_function)) Scope(void (*marker)())
        : m_marker{reinterpret_cast<void *>(marker)} {
        LN_PROF_ENTER(m_marker);
    }
    __attribute__((no_instrument_function)) ~Scope() { LN_PROF_EXIT(m_marker); }

private:
    void *m_marker;
};

} // namespace ln::prof
#define LN_PROF_CONCAT_IMPL(x, y) x##y
#define LN_PROF_CONCAT(x, y) LN_PROF_CONCAT_IMPL(x, y)
#define LN_PROF_SCOPE()                                                        \
    struct LN_PROF_CONCAT(ln_prof_scope_marker_, __LINE__) {                   \
        __attribute__((noipa, used, no_instrument_function)) static void       \
        fn() {}                                                                \
    };                                                                         \
    ln::prof::Scope LN_PROF_CONCAT(ln_prof_scope_, __LINE__) {                 \
        &LN_PROF_CONCAT(ln_prof_scope_marker_, __LINE__)::fn                   \
    }

#endif // __cplusplus
#endif
