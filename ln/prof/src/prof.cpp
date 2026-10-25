// Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/prof/prof.h"

#define FORCE_INLINE inline __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))

#ifndef LN_PROF_ITM_PORT
#define LN_PROF_ITM_PORT 0
#endif

#include "ln/port/itm.h"

#include "FreeRTOS.h"
#include "task.h"

#include <bitset>
#include <cstdint>

#ifdef LN_PROF_STATS
#include "ln/shell/CLI.hpp"
#endif

struct Packet {

    struct ProfileEvent {
        enum class State : uint32_t {
            enter = 0,
            exit = 1,
        };

        enum class Type : uint32_t {
            thread = 0,
            fn = 1,
        };
    };

    enum class Id : uint32_t {
        /**
         * @brief Packet ID for sending profile events.
         *
         *  Packet structure:
         *    - pkt_word0[7:0]: Packet ID (PROFILE_EVENT const value)
         *    - pkt_word0[31:8]: Cycle count delta since last event
         *    - pkt_word1[6:0]: Context ID
         *    - pkt_word1[7]: ProfileEvent::State
         *    - pkt_word1[8]: ProfileEvent::Type
         *    - pkt_word1[31:9]: Bits [23:1] of the function pointer for
         * function events, unused for task events (bit 0 is the Thumb bit and
         * the most significant byte of a 32-bit address is typically never
         *      changing, so they are omitted and parametrized on client side).
         */
        PROFILE_EVENT = 0xA5,

        /**
         * @brief Packet ID for disclosing context names.
         *
         *  Packet structure:
         *    - pkt_word0[7:0]: Packet ID (DISCLOSE_CONTEXT_NAME const value)
         *    - pkt_word0[31:8]: Context ID
         *    - pkt_word1[7:0]: First two characters of the context name
         *    - word2[31:0]: Subsequent characters of the context name (if any)
         */
        DISCLOSE_CONTEXT_NAME = 0xA6,
    };
};

volatile bool started = false;
volatile bool profile_interrupts = true;
static uint32_t last_cycle_count = 0;
// Context ids double as Tracy thread ids; 0 is also "task number unassigned".
enum ContextId : uint32_t {
    SCHEDULER = 1,
    INTERRUPT = 2,
    FIRST_TASK = 3,
    /* subsequent tasks */
};
static uint32_t next_context_id = ContextId::FIRST_TASK;
static volatile uint32_t current_task_context_id = 0;
static std::bitset<128> disclosed_name_contexts;
static uint32_t last_announce_reset = 0;

#ifdef LN_PROF_STATS
static struct {
    uint32_t events;                // profile events emitted
    uint32_t event_cycles;          // total time emitting events, incl. stalls
    uint32_t event_cycles_min;      // shortest single event
    uint32_t event_cycles_max;      // longest single event
    uint32_t fifo_stalls;           // ITM FIFO waits
    uint32_t fifo_stall_cycles;     // total time waiting on the FIFO
    uint32_t fifo_stall_cycles_min; // shortest single wait
    uint32_t fifo_stall_cycles_max; // longest single wait
} stats;
static uint32_t stats_begin_cycles = 0;

static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_word(uint32_t value) {
    if (!LN_ITM_IS_FIFO_READY(LN_PROF_ITM_PORT)) {
        const uint32_t stall_begin = DWT->CYCCNT;
        LN_ITM_BLOCK_WAIT_FOR_FIFO(LN_PROF_ITM_PORT);
        stats.fifo_stalls++;
        const uint32_t stall_cycles = DWT->CYCCNT - stall_begin;
        stats.fifo_stall_cycles += stall_cycles;
        if (stall_cycles < stats.fifo_stall_cycles_min) {
            stats.fifo_stall_cycles_min = stall_cycles;
        }
        if (stall_cycles > stats.fifo_stall_cycles_max) {
            stats.fifo_stall_cycles_max = stall_cycles;
        }
    }
    LN_ITM_SEND_WORD_UNSAFE(LN_PROF_ITM_PORT, value);
}
#define _LN_ITM_SEND_WORD(value) ln_prof_send_word(value)

static void ln_prof_stats_reset() {
    const uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    stats = {};
    stats.event_cycles_min = UINT32_MAX;
    stats.fifo_stall_cycles_min = UINT32_MAX;
    stats_begin_cycles = DWT->CYCCNT;
    __set_PRIMASK(old_primask);
}
#else
#define _LN_ITM_SEND_WORD(value) LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, value)
#endif

static LN_PROF_ATTR void ln_prof_send_context_name(uint32_t context_id,
                                                   const char *name) {
    _LN_ITM_SEND_WORD(
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
        _LN_ITM_SEND_WORD(word);
    }
}

/**
 * @note Shall be called with interrupts masked, may send a context name.
 */
static LN_PROF_ATTR NO_INLINE uint32_t ln_prof_resolve_task_context_id() {
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if (!task) {
        return 0;
    }
    UBaseType_t id = uxTaskGetTaskNumber(task);
    if (id == 0) {
        id = next_context_id;
        next_context_id = next_context_id < 0x7E ? next_context_id + 1
                                                 : ContextId::FIRST_TASK;
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

bool LN_PROF_ATTR ln_prof_start(ln_prof_config_t config) {

    profile_interrupts = config.profile_interrupts == 1;

    // Enable trace (required for ITM and DWT)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // Reset and enable cycle counter (used by ln/prof for timestamps)
    DWT->CYCCNT = 0;
    last_cycle_count = 0;
    last_announce_reset = 0;
    disclosed_name_contexts.reset();
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#ifdef LN_PROF_STATS
    ln_prof_stats_reset();
#endif
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
    const uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    current_task_context_id = ln_prof_resolve_task_context_id();
    started = true;
    __set_PRIMASK(old_primask);
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
 * - Project-wide LTO could let the compiler inline across these TU boundaries
 *   safely, but that's a broad build-system change with its own risk/build-time
 *   tradeoffs, not scoped to this file.
 */
static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_profile_event(
    Packet::ProfileEvent::Type type, Packet::ProfileEvent::State state,
    void *fn = nullptr) {
#ifdef LN_PROF_STATS
    const uint32_t stats_entry_cycles = DWT->CYCCNT;
#endif
    const bool inside_interrupt = __get_IPSR() != 0;
    if (!profile_interrupts && inside_interrupt) {
        return;
    }
    uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    // Task switch hooks may run in PendSV but belong to the task being
    // switched.
    const bool switched_in = type == Packet::ProfileEvent::Type::thread &&
                             state == Packet::ProfileEvent::State::exit;
    if (switched_in) {
        current_task_context_id = ln_prof_resolve_task_context_id();
    }
    const uint32_t context_id =
        inside_interrupt && type != Packet::ProfileEvent::Type::thread
            ? ContextId::INTERRUPT
            : current_task_context_id;
    const uint32_t cycle_count = DWT->CYCCNT;
    const uint32_t pkt_word0 =
        static_cast<uint32_t>(Packet::Id::PROFILE_EVENT) |
        ((cycle_count - last_cycle_count) << 8);
    last_cycle_count = cycle_count;
    _LN_ITM_SEND_WORD(pkt_word0);
    const uint32_t pkt_word1 =
        context_id | (static_cast<uint32_t>(state) << 7) |
        (static_cast<uint32_t>(type) << 8) |
        (((reinterpret_cast<uint32_t>(fn) >> 1) & 0x7FFFFF) << 9);
    _LN_ITM_SEND_WORD(pkt_word1);
#ifdef LN_PROF_STATS
    const uint32_t event_cycles = DWT->CYCCNT - stats_entry_cycles;
    stats.events++;
    stats.event_cycles += event_cycles;
    if (event_cycles < stats.event_cycles_min) {
        stats.event_cycles_min = event_cycles;
    }
    if (event_cycles > stats.event_cycles_max) {
        stats.event_cycles_max = event_cycles;
    }
#endif
    __set_PRIMASK(old_primask);
}

static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_profile_enter_event(
    void *this_fn) {
    if (!started) {
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
    ln_prof_send_profile_event(Packet::ProfileEvent::Type::fn,
                               Packet::ProfileEvent::State::exit, this_fn);
}

extern "C" LN_PROF_ATTR void ln_prof_task_switched_out() {
    if (started) {
        ln_prof_send_profile_event(Packet::ProfileEvent::Type::thread,
                                   Packet::ProfileEvent::State::enter);
    }
}

extern "C" LN_PROF_ATTR void ln_prof_task_switched_in() {
    if (started) {
        ln_prof_send_profile_event(Packet::ProfileEvent::Type::thread,
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

#ifdef LN_PROF_STATS
namespace ln::shell {

static float cycles_to_us(uint32_t cycles) {
    return static_cast<float>(cycles) * 1e6f /
           static_cast<float>(SystemCoreClock);
}

static float percent(uint32_t part, uint32_t whole) {
    return whole ? 100.0f * static_cast<float>(part) / static_cast<float>(whole)
                 : 0.0f;
}

Cmd cmd_prof{
    Cmd::Cfg{.name = "prof", .short_description = "profiler commands"}};

Cmd cmd_prof_stats{Cmd::Cfg{
    .parent_cmd = &cmd_prof,
    .name = "stats",
    .short_description = "show profiler overhead statistics",
    .long_description = R"(Usage:
  prof stats          show statistics since profile start or last reset
  prof stats reset    restart the measurement window

The measurement itself adds some time per event.
)",
    .fn = [](Cmd::Ctx ctx) {
        using namespace std::literals::string_view_literals;
        if (ctx.args.size() == 1 && ctx.args[0] == "reset"sv) {
            ln_prof_stats_reset();
            return Err::ok;
        }
        if (!ctx.args.empty()) {
            return Err::badArg;
        }
        const uint32_t old_primask = __get_PRIMASK();
        __disable_irq();
        const auto s = stats;
        const uint32_t elapsed = DWT->CYCCNT - stats_begin_cycles;
        __set_PRIMASK(old_primask);
        ctx.cli.print("events:            {}\n", s.events);
        ctx.cli.print("time/event min:    {:.2f} us\n",
                      cycles_to_us(s.events ? s.event_cycles_min : 0));
        ctx.cli.print("time/event avg:    {:.2f} us\n",
                      cycles_to_us(s.events ? s.event_cycles / s.events : 0));
        ctx.cli.print("time/event max:    {:.2f} us\n",
                      cycles_to_us(s.event_cycles_max));
        ctx.cli.print("fifo stalls:       {}\n", s.fifo_stalls);
        ctx.cli.print(
            "fifo stall min:    {:.2f} us\n",
            cycles_to_us(s.fifo_stalls ? s.fifo_stall_cycles_min : 0));
        ctx.cli.print("fifo stall avg:    {:.2f} us\n",
                      cycles_to_us(s.fifo_stalls
                                       ? s.fifo_stall_cycles / s.fifo_stalls
                                       : 0));
        ctx.cli.print("fifo stall max:    {:.2f} us\n",
                      cycles_to_us(s.fifo_stall_cycles_max));
        ctx.cli.print("fifo stall total:  {:.2f} us ({:.1f}% of event time)\n",
                      cycles_to_us(s.fifo_stall_cycles),
                      percent(s.fifo_stall_cycles, s.event_cycles));
        ctx.cli.print("cpu in profiler:   {:.1f}% of {:.1f} ms\n",
                      percent(s.event_cycles, elapsed),
                      cycles_to_us(elapsed) / 1000.0f);
        return Err::ok;
    }}};

} // namespace ln::shell
#endif
