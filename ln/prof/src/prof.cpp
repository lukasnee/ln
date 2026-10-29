// Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/prof/prof.h"

#define FORCE_INLINE inline __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

// Section for the event hot path, e.g. ".ccmram" to run it with no flash waits.
#ifdef LN_PROF_HOT_SECTION
#define LN_PROF_HOT __attribute__((section(LN_PROF_HOT_SECTION)))
#else
#define LN_PROF_HOT
#endif

#ifndef LN_PROF_ITM_BASE_PORT
#define LN_PROF_ITM_BASE_PORT 0
#endif

// Bytes of code from the start of flash that can be profiled.
#ifndef LN_PROF_FW_SIZE
#define LN_PROF_FW_SIZE 0x80000
#endif

#include "ln/port/itm.h"

#ifdef LN_PROF_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#include <bitset>
#include <cstdint>

#ifdef LN_PROF_STATS
#include "ln/shell/CLI.hpp"
#endif

enum ItmPort : uint32_t {

    FN_ENTER_THREAD = LN_PROF_ITM_BASE_PORT,
    FN_ENTER_ISR,
    FN_EXIT_THREAD,
    FN_EXIT_ISR,
    SWITCH_IN,
    SWITCH_OUT,
    TIME,
    NAME,
    NAME_MORE,
    FN_BASE,

    _COUNT,

    _MASK = ((1u << _COUNT) - 1) << LN_PROF_ITM_BASE_PORT
};
static_assert(LN_PROF_ITM_BASE_PORT + ItmPort::_COUNT <= 32,
              "ln/prof ITM port mapping (LN_PROF_ITM_BASE_PORT) does not fit "
              "within available 32 ports");

// Function address bits [N:1] in an enter word, enough for LN_PROF_FW_SIZE.
constexpr uint32_t ln_prof_fn_addr_bits(uint64_t fw_size) {
    uint32_t bits = 0;
    while ((uint64_t{1} << (bits + 1)) < fw_size) {
        bits++;
    }
    return bits;
}
constexpr uint32_t LN_PROF_FN_ADDR_BITS = ln_prof_fn_addr_bits(LN_PROF_FW_SIZE);
static_assert(LN_PROF_FN_ADDR_BITS >= 4 && LN_PROF_FN_ADDR_BITS <= 24,
              "LN_PROF_FW_SIZE is out of range");

// Bits of the context ID in a switch value.
constexpr uint32_t LN_PROF_CONTEXT_ID_BITS = 7;

static constexpr uint32_t LN_PROF_FN_CYCLE_COUNT_DELTA_BITS =
    32 - LN_PROF_FN_ADDR_BITS;
static constexpr uint32_t LN_PROF_FN_CYCCNT_DELTA_MAX =
    (1u << LN_PROF_FN_CYCLE_COUNT_DELTA_BITS) - 1;

extern "C" void ln_prof_thread_switched_in();

volatile bool started = false;
volatile bool profile_interrupts = true;
static uint32_t last_cycle_count = 0;
// Context ids double as Tracy thread ids; 0 is also "thread number unassigned".
enum ContextId : uint32_t {
    SCHEDULER = 1,
    INTERRUPT = 2,
    FIRST_THREAD = 3,
    /* subsequent threads */
};
#ifdef LN_PROF_RTOS
static uint32_t next_context_id = ContextId::FIRST_THREAD;
#endif
static std::bitset<128> disclosed_name_contexts;
static uint32_t last_announce_reset = 0;
// The FN_BASE last sent, UINT32_MAX (never a valid one) to send it again.
static uint32_t announced_fn_base = UINT32_MAX;

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

static void ln_prof_stats_reset() {
    const uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    stats = {};
    stats.event_cycles_min = UINT32_MAX;
    stats.fifo_stall_cycles_min = UINT32_MAX;
    stats_begin_cycles = DWT->CYCCNT;
    __set_PRIMASK(old_primask);
}
#endif

static LN_PROF_ATTR FORCE_INLINE void ln_prof_wait_for_fifo(uint32_t port) {
#ifdef LN_PROF_STATS
    if (UNLIKELY(!LN_ITM_IS_FIFO_READY(port))) {
        const uint32_t stall_begin = DWT->CYCCNT;
        LN_ITM_BLOCK_WAIT_FOR_FIFO(port);
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
#else
    LN_ITM_BLOCK_WAIT_FOR_FIFO(port);
#endif
}

static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_word(uint32_t port,
                                                        uint32_t value) {
    ln_prof_wait_for_fifo(port);
    LN_ITM_SEND_WORD_UNSAFE(port, value);
}

static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_narrow(uint32_t port,
                                                          uint32_t value) {
    ln_prof_wait_for_fifo(port);
    if (value <= UINT8_MAX) {
        LN_ITM_SEND_BYTE_UNSAFE(port, value);
    }
    else if (value <= UINT16_MAX) {
        LN_ITM_SEND_HALFWORD_UNSAFE(port, value);
    }
    else {
        LN_ITM_SEND_WORD_UNSAFE(port, value);
    }
}

static LN_PROF_ATTR void ln_prof_send_context_name(uint32_t context_id,
                                                   const char *name) {
    uint32_t word = context_id;
    bool terminated = false;
    for (uint32_t i = 0; i < 3 && !terminated; i++) {
        const char c = name[i];
        word |= static_cast<uint32_t>(static_cast<uint8_t>(c)) << (8 + 8 * i);
        terminated = (c == '\0');
    }
    ln_prof_send_word(ItmPort::NAME, word);
    for (uint32_t offset = 3; !terminated; offset += 4) {
        word = 0;
        for (uint32_t i = 0; i < 4 && !terminated; i++) {
            const char c = name[offset + i];
            word |= static_cast<uint32_t>(static_cast<uint8_t>(c)) << (8 * i);
            terminated = (c == '\0');
        }
        ln_prof_send_word(ItmPort::NAME_MORE, word);
    }
}

/**
 * @note Shall be called with interrupts masked, may send a context name.
 */
static LN_PROF_ATTR NO_INLINE uint32_t ln_prof_resolve_thread_context_id() {
#ifdef LN_PROF_RTOS
    TaskHandle_t thread = xTaskGetCurrentTaskHandle();
    if (UNLIKELY(!thread)) {
        return 0;
    }
    UBaseType_t id = uxTaskGetTaskNumber(thread);
    if (UNLIKELY(id == 0)) {
        id = next_context_id;
        next_context_id = next_context_id < 0x7E ? next_context_id + 1
                                                 : ContextId::FIRST_THREAD;
        vTaskSetTaskNumber(thread, id);
    }
    id &= 0x7F;
    const char *name = pcTaskGetName(thread);
#else
    // Bare metal: a single thread context.
    const uint32_t id = ContextId::FIRST_THREAD;
    const char *name = "main";
#endif
    // Re-announce context names and function base about once per second so
    // late-attaching recorders are informed
    const uint32_t now = DWT->CYCCNT;
    if (UNLIKELY(now - last_announce_reset > SystemCoreClock)) {
        last_announce_reset = now;
        disclosed_name_contexts.reset();
        announced_fn_base = UINT32_MAX;
    }
    if (UNLIKELY(!disclosed_name_contexts.test(id))) {
        disclosed_name_contexts.set(id);
        ln_prof_send_context_name(id, name);
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
    announced_fn_base = UINT32_MAX;
    disclosed_name_contexts.reset();
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#ifdef LN_PROF_STATS
    ln_prof_stats_reset();
#endif
    // Unlock ITM
    ITM->LAR = 0xC5ACCE55;
    // Enable ITM
    ITM->TCR |= ITM_TCR_ITMENA_Msk;
    // Enable the stimulus ports of ln/prof
    ITM->TER |= ItmPort::_MASK;

    const uint32_t timeout = SystemCoreClock / 1000U;
    const uint32_t start_time = DWT->CYCCNT;
    bool ready = false;
    while ((DWT->CYCCNT - start_time) < timeout) {
        if (LN_ITM_IS_PORT_READY(LN_PROF_ITM_BASE_PORT) &&
            LN_ITM_IS_FIFO_READY(LN_PROF_ITM_BASE_PORT)) {
            ready = true;
            break;
        }
    }
    if (!ready) {
        ITM->TER &= ~ItmPort::_MASK; // undo
        return false;
    }
    const uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    started = true;
    ln_prof_thread_switched_in(); // tell the host about the current thread
                                  // context
    __set_PRIMASK(old_primask);
    return true;
}

void LN_PROF_ATTR ln_prof_stop() {
    ITM->TER &= ~ItmPort::_MASK;

    started = false;
}

/**
 * The critical section of one event: interrupts masked from begin to end, the
 * cycle count delta since the last event taken at begin.
 */
struct ProfEvent {
    bool inside_interrupt;
    uint32_t old_primask;
    uint32_t cycle_count_delta;
#ifdef LN_PROF_STATS
    uint32_t entry_cycles;
#endif
};

/**
 * @attention Performance is of utmost importance!
 *
 * @todo Research further optimizations for __cyg_profile_func_enter/exit if
 * there's motivation to squeeze more:
 * - Project-wide LTO could let the compiler inline across these TU boundaries
 *   safely, but that's a broad build-system change with its own risk/build-time
 *   tradeoffs, not scoped to this file.
 *
 * @return false if there is no event to send, then ln_prof_event_end() shall
 * not be called.
 */
static LN_PROF_ATTR FORCE_INLINE bool ln_prof_event_begin(ProfEvent &event) {
    if (UNLIKELY(!started)) {
        return false;
    }
#ifdef LN_PROF_STATS
    event.entry_cycles = DWT->CYCCNT;
#endif
    event.inside_interrupt = __get_IPSR() != 0;
    if (UNLIKELY(!profile_interrupts && event.inside_interrupt)) {
        return false;
    }
    event.old_primask = __get_PRIMASK();
    __disable_irq();
    const uint32_t cycle_count = DWT->CYCCNT;
    event.cycle_count_delta = cycle_count - last_cycle_count;
    last_cycle_count = cycle_count;
    return true;
}

static LN_PROF_ATTR FORCE_INLINE void ln_prof_event_end(
    const ProfEvent &event) {
#ifdef LN_PROF_STATS
    const uint32_t event_cycles = DWT->CYCCNT - event.entry_cycles;
    stats.events++;
    stats.event_cycles += event_cycles;
    if (event_cycles < stats.event_cycles_min) {
        stats.event_cycles_min = event_cycles;
    }
    if (event_cycles > stats.event_cycles_max) {
        stats.event_cycles_max = event_cycles;
    }
#endif
    __set_PRIMASK(event.old_primask);
}

// May run in PendSV, the thread switched to is the one current in the kernel.
extern "C" LN_PROF_ATTR LN_PROF_HOT void ln_prof_thread_switched_in() {
    ProfEvent event;
    if (UNLIKELY(!ln_prof_event_begin(event))) {
        return;
    }
    const uint32_t context_id = ln_prof_resolve_thread_context_id();
    uint32_t cycle_count_delta = event.cycle_count_delta;
    if (UNLIKELY(cycle_count_delta > LN_PROF_FN_CYCCNT_DELTA_MAX)) {
        /* more time passed than can fit in the bitfield - send cycle count on
               the TIME port instead with wider range */
        ln_prof_send_narrow(ItmPort::TIME, cycle_count_delta);
        cycle_count_delta = 0;
    }
    ln_prof_send_narrow(ItmPort::SWITCH_IN,
                        (cycle_count_delta << LN_PROF_CONTEXT_ID_BITS) |
                            context_id);
    ln_prof_event_end(event);
}

extern "C" LN_PROF_ATTR LN_PROF_HOT void ln_prof_thread_switched_out() {
    ProfEvent event;
    if (UNLIKELY(!ln_prof_event_begin(event))) {
        return;
    }
    ln_prof_send_narrow(ItmPort::SWITCH_OUT, event.cycle_count_delta);
    ln_prof_event_end(event);
}

static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_enter(void *fn) {
    ProfEvent event;
    if (UNLIKELY(!ln_prof_event_begin(event))) {
        return;
    }
    const uint32_t fn_address = reinterpret_cast<uint32_t>(fn);
    const uint32_t fn_base =
        fn_address & ~((1u << (LN_PROF_FN_ADDR_BITS + 1)) - 1);
    if (UNLIKELY(fn_base != announced_fn_base)) {
        announced_fn_base = fn_base;
        ln_prof_send_word(ItmPort::FN_BASE, fn_base | LN_PROF_FN_ADDR_BITS);
    }
    uint32_t cycle_count_delta = event.cycle_count_delta;
    if (UNLIKELY(cycle_count_delta > LN_PROF_FN_CYCCNT_DELTA_MAX)) {
        /* more time passed than can fit in the bitfield - send cycle count on
        the TIME port instead with wider range */
        ln_prof_send_narrow(ItmPort::TIME, cycle_count_delta);
        cycle_count_delta = 0;
    }
    const uint32_t fn_bits = (reinterpret_cast<uint32_t>(fn) >> 1) &
                             ((1u << LN_PROF_FN_ADDR_BITS) - 1);
    ln_prof_send_word(event.inside_interrupt ? ItmPort::FN_ENTER_ISR
                                             : ItmPort::FN_ENTER_THREAD,
                      (cycle_count_delta << LN_PROF_FN_ADDR_BITS) | fn_bits);
    ln_prof_event_end(event);
}

static LN_PROF_ATTR FORCE_INLINE void ln_prof_send_exit() {
    ProfEvent event;
    if (UNLIKELY(!ln_prof_event_begin(event))) {
        return;
    }
    ln_prof_send_narrow(event.inside_interrupt ? ItmPort::FN_EXIT_ISR
                                               : ItmPort::FN_EXIT_THREAD,
                        event.cycle_count_delta);
    ln_prof_event_end(event);
}

extern "C" LN_PROF_ATTR LN_PROF_HOT void ln_prof_scope_enter(void *marker) {
    ln_prof_send_enter(marker);
}

extern "C" LN_PROF_ATTR LN_PROF_HOT void ln_prof_scope_exit(
    [[maybe_unused]] void *marker) {
    ln_prof_send_exit();
}

extern "C" LN_PROF_ATTR LN_PROF_HOT void __cyg_profile_func_enter(
    void *this_fn, [[maybe_unused]] void *call_site) {
    ln_prof_send_enter(this_fn);
}

extern "C" LN_PROF_ATTR LN_PROF_HOT void __cyg_profile_func_exit(
    [[maybe_unused]] void *this_fn, [[maybe_unused]] void *call_site) {
    ln_prof_send_exit();
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
