// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#ifndef LN_PROF

#define LN_PROF_START()
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

    extern "C" LN_PROF_ATTR bool ln_prof_start();
#define LN_PROF_START() ln_prof_start()
    extern "C" LN_PROF_ATTR void ln_prof_stop();
#define LN_PROF_STOP() ln_prof_stop()

    extern "C" LN_PROF_ATTR void __cyg_profile_func_enter(void *this_fn,
                                                          void *call_site);
#define LN_PROF_ENTER(id) __cyg_profile_func_enter((void *)id, NULL)

    extern "C" LN_PROF_ATTR void __cyg_profile_func_exit(void *this_fn,
                                                         void *call_site);
#define LN_PROF_EXIT(id) __cyg_profile_func_exit((void *)id, NULL)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace ln::prof {

class Scope {
public:
    __attribute__((no_instrument_function)) Scope(int id) : m_id{id} {
        LN_PROF_ENTER(m_id);
    }
    __attribute__((no_instrument_function)) ~Scope() { LN_PROF_EXIT(m_id); }

private:
    int m_id;
};

} // namespace ln::prof
#define LN_PROF_CONCAT_IMPL(x, y) x##y
#define LN_PROF_CONCAT(x, y) LN_PROF_CONCAT_IMPL(x, y)
#define LN_PROF_SCOPE()                                                        \
    ln::prof::Scope LN_PROF_CONCAT(__ln_prof_scope_, __LINE__) { __LINE__ }

#endif // __cplusplus
#endif
