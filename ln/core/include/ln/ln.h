// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <string.h> // for strrchr

#ifdef __cplusplus
extern "C"
{
#endif

#define LN_FILENAME                                                            \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

    void ln_panic(const char *file, int line, const char *message);

#define LN_PANIC() ln_panic(LN_FILENAME, __LINE__, nullptr)

#define LN_PANIC_WITH_MSG(message) ln_panic(LN_FILENAME, __LINE__, message)

#define LN_ASSERT(expr, on_failure)                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            on_failure;                                                        \
        }                                                                      \
    } while (0)
#define LN_ASSERT_PANIC(expr) LN_ASSERT(expr, LN_PANIC())

#define LN_CHECK(expr, var, cond, on_failure, on_success)                      \
    do {                                                                       \
        const auto var = (expr);                                               \
        if (cond) {                                                            \
            on_failure;                                                        \
            return var;                                                        \
        }                                                                      \
        on_success;                                                            \
    } while (0)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <chrono>
#include <string_view>

namespace ln {

[[noreturn]] void panic(const char *file, int line, const char *message);

/**
 * @brief Reset the system (software reset).
 *
 * @note Port-specific implementation.
 */
[[noreturn]] void reset();

/**
 * @brief Get the system uptime in milliseconds.
 *
 * @note Port-specific implementation.
 */
std::chrono::milliseconds get_uptime_ms();

template <typename Rep, typename Period>
void sleep(std::chrono::duration<Rep, Period> duration) {
    // NOLINTNEXTLINE(readability-redundant-declaration)
    void sleep(std::chrono::milliseconds duration);
    sleep(std::chrono::duration_cast<std::chrono::milliseconds>(duration));
}

void sleep_until(std::chrono::time_point<std::chrono::steady_clock> time_point);

/**
 * @brief Check if the current context is an interrupt context.
 *
 * @note Port-specific implementation.
 */
bool interrupt_context();

/**
 * @brief Disable interrupts.
 *
 * @note Port-specific implementation.
 */
void disable_irq();

/**
 * @brief Enable interrupts.
 *
 * @note Port-specific implementation.
 */
void enable_irq();

/**
 * @brief Get the name of the current thread. Invalid if interrupt_context() is
 * true.
 *
 * @note Port-specific implementation.
 */
std::string_view get_thread_name();

} // namespace ln

#endif // __cplusplus
