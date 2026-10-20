// Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/prof/prof.h"

#define FORCE_INLINE inline __attribute__((always_inline))

#ifndef LN_PROF_ITM_PORT
#define LN_PROF_ITM_PORT 0
#endif

#include "ln/port/itm.h"

#include "FreeRTOS.h"
#include "task.h"

#include <bitset>
#include <cstdint>

struct Packet {

    struct ProfileEvent {
        enum class State : uint32_t {
            enter = 0,
            exit = 1,
        };

        enum class Type : uint32_t {
            task = 0,
            fn = 1,
        };
    };

    enum class Id : uint32_t {
        /**
         * @brief Packet ID for sending profile events.
         *
         *  Packet structure:
         *    - word0[7:0]: Packet ID (PROFILE_EVENT const value)
         *    - word0[31:8]: Cycle count delta since last event
         *    - word1[6:0]: Context ID
         *    - word1[7]: ProfileEvent::State
         *    - word1[8]: ProfileEvent::Type
         *    - word1[31:9]: Bits [23:1] of the function pointer for function
         *      events, unused for task events (bit 0 is the Thumb bit and the
         *      most significant byte of a 32-bit address is typically never
         *      changing, so they are omitted and parametrized on client side).
         */
        PROFILE_EVENT = 0xA5,

        /**
         * @brief Packet ID for disclosing context names.
         *
         *  Packet structure:
         *    - word0[7:0]: Packet ID (DISCLOSE_CONTEXT_NAME const value)
         *    - word0[31:8]: Context ID
         *    - word1[7:0]: First two characters of the context name
         *    - word2[31:0]: Subsequent characters of the context name (if any)
         */
        DISCLOSE_CONTEXT_NAME = 0xA6,
    };
};

volatile bool started = false;
static uint32_t last_cycle_count = 0;
static uint32_t next_context_id = 1;
static std::bitset<128> disclosed_name_contexts;
static uint32_t last_announce_reset = 0;

static LN_PROF_ATTR void ln_prof_send_context_name(uint32_t context_id,
                                                   const char *name) {
    LN_ITM_SEND_WORD(
        LN_PROF_ITM_PORT,
        static_cast<uint32_t>(Packet::Id::DISCLOSE_CONTEXT_NAME) |
            (context_id << 8) |
            (static_cast<uint32_t>(static_cast<uint8_t>(name[0])) << 16) |
            (static_cast<uint32_t>(static_cast<uint8_t>(name[1])) << 24));
    bool terminated = name[0] == '\0' || name[1] == '\0';
    for (uint32_t offset = 2; !terminated; offset += 4) {
        uint32_t word = 0;
        for (uint32_t i = 0; i < 4 && !terminated; i++) {
            const char c = name[offset + i];
            word |= static_cast<uint32_t>(static_cast<uint8_t>(c)) << (8 * i);
            terminated = (c == '\0');
        }
        LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word);
    }
}

static LN_PROF_ATTR FORCE_INLINE uint32_t ln_prof_get_current_context_id() {
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if (task == nullptr) {
        return 0;
    }
    UBaseType_t id = uxTaskGetTaskNumber(task);
    if (id == 0) {
        id = next_context_id;
        next_context_id = (next_context_id % 0x7F) + 1;
        vTaskSetTaskNumber(task, id);
    }
    id &= 0x7F;
    // Re-announce about once per second so late-attaching recorders get names.
    const uint32_t now = DWT->CYCCNT;
    if (now - last_announce_reset > SystemCoreClock) {
        last_announce_reset = now;
        disclosed_name_contexts.reset();
    }
    if (!disclosed_name_contexts.test(id)) {
        disclosed_name_contexts.set(id);
        ln_prof_send_context_name(id, pcTaskGetName(task));
    }
    return id;
}

bool LN_PROF_ATTR ln_prof_start() {

    // Enable trace (required for ITM and DWT)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // Reset and enable cycle counter (used by ln/prof for timestamps)
    DWT->CYCCNT = 0;
    last_cycle_count = 0;
    last_announce_reset = 0;
    disclosed_name_contexts.reset();
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    // Unlock ITM
    ITM->LAR = 0xC5ACCE55;
    // Enable ITM
    ITM->TCR |= ITM_TCR_ITMENA_Msk;
    // Enable stimulus port 0 (LN_PROF_ITM_PORT)
    ITM->TER |= (1UL << LN_PROF_ITM_PORT);

    const uint32_t timeout = SystemCoreClock / 1000U;
    const uint32_t start_time = DWT->CYCCNT;
    bool ready = false;
    while ((DWT->CYCCNT - start_time) < timeout) {
        if (LN_ITM_IS_PORT_READY(LN_PROF_ITM_PORT) &&
            LN_ITM_IS_FIFO_READY(LN_PROF_ITM_PORT)) {
            ready = true;
            break;
        }
    }
    if (!ready) {
        ITM->TER &= ~(1UL << LN_PROF_ITM_PORT); // undo
        return false;
    }
    started = true;
    return true;
}

void LN_PROF_ATTR ln_prof_stop() {
    ITM->TER &= ~(1UL << LN_PROF_ITM_PORT);

    started = false;
}

/**
 * @attention Performance is of utmost importance!
 *
 * @todo Research further optimizations for __cyg_profile_func_enter/exit if
 * there's motivation to squeeze more:
 * - xTaskGetCurrentTaskHandle()/uxTaskGetTaskNumber() are still real calls;
 *   avoiding them means reaching into FreeRTOS's private TCB_t layout (fragile,
 *   breaks on kernel/config changes) - not done so far.
 * - Project-wide LTO could let the compiler inline across these TU boundaries
 *   safely, but that's a broad build-system change with its own risk/build-time
 *   tradeoffs, not scoped to this file.
 */
static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_profile_event(
    Packet::ProfileEvent::Type type, Packet::ProfileEvent::State state,
    void *fn = nullptr) {
    uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    const uint32_t context_id = ln_prof_get_current_context_id();
    const uint32_t cycle_count = DWT->CYCCNT;
    const uint32_t word0 = static_cast<uint32_t>(Packet::Id::PROFILE_EVENT) |
                           ((cycle_count - last_cycle_count) << 8);
    last_cycle_count = cycle_count;
    LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word0);
    const uint32_t word1 =
        context_id | (static_cast<uint32_t>(state) << 7) |
        (static_cast<uint32_t>(type) << 8) |
        (((reinterpret_cast<uint32_t>(fn) >> 1) & 0x7FFFFF) << 9);
    LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word1);
    __set_PRIMASK(old_primask);
}

static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_profile_enter_event(
    void *this_fn) {
    if (!started) {
        return;
    }
    if (xPortIsInsideInterrupt()) {
        return;
    }
    ln_prof_send_profile_event(Packet::ProfileEvent::Type::fn,
                               Packet::ProfileEvent::State::enter, this_fn);
}

static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_profile_exit_event(
    void *this_fn) {
    if (!started) {
        return;
    }
    if (xPortIsInsideInterrupt()) {
        return;
    }
    ln_prof_send_profile_event(Packet::ProfileEvent::Type::fn,
                               Packet::ProfileEvent::State::exit, this_fn);
}

extern "C" LN_PROF_ATTR void ln_prof_task_switched_out() {
    if (started) {
        ln_prof_send_profile_event(Packet::ProfileEvent::Type::task,
                                   Packet::ProfileEvent::State::enter);
    }
}

extern "C" LN_PROF_ATTR void ln_prof_task_switched_in() {
    if (started) {
        ln_prof_send_profile_event(Packet::ProfileEvent::Type::task,
                                   Packet::ProfileEvent::State::exit);
    }
}

extern "C" LN_PROF_ATTR void __cyg_profile_func_enter(
    void *this_fn, [[maybe_unused]] void *call_site) {
    ln_prof_send_profile_enter_event(this_fn);
}

extern "C" LN_PROF_ATTR void __cyg_profile_func_exit(
    void *this_fn, [[maybe_unused]] void *call_site) {
    ln_prof_send_profile_exit_event(this_fn);
}
