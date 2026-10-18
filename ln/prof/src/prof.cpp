// Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/prof/prof.h"

#ifndef LN_PROF_ITM_PORT
#define LN_PROF_ITM_PORT 0
#endif

#include "ln/port/itm.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cstdint>
#include <stdint.h>

// Wire format reference. Built manually in the hot path below (see
// __cyg_profile_func_enter/exit) to avoid the stack round-trips gcc emits
// when lowering this bitfield struct.
struct Packet {
    enum class Type : uint32_t {
        enter = 0,
        exit = 1,
    };

    static constexpr std::uint8_t SYNC_BYTE = 0xA5;

#pragma pack(push, 1)
    union {
        struct {
            uint32_t sync_byte : 8;
            uint32_t cycle_cnt_delta : 24;
            uint32_t type : 1;
            uint32_t context : 7;
            uint32_t fn : 24; // MSB shall be provided by the other packet
        };
        uint32_t as_u32[2];
    };
#pragma pack(pop)
};

volatile bool started = false;
static uint32_t last_cycle_count = 0;

bool ln_prof_start() {

    // Enable trace (required for ITM and DWT)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // Reset and enable cycle counter (used by ln/prof for timestamps)
    DWT->CYCCNT = 0;
    last_cycle_count = 0;
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

void ln_prof_stop() {
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
extern "C" LN_PROF_ATTR void __cyg_profile_func_enter(void *this_fn,
                                                      void *call_site) {
    (void)call_site;
    if (!started) {
        return;
    }
    if (xPortIsInsideInterrupt()) {
        return;
    }
    uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    const uint32_t cycle_count = DWT->CYCCNT;
    const uint32_t word0 =
        Packet::SYNC_BYTE | ((cycle_count - last_cycle_count) << 8);
    last_cycle_count = cycle_count;
    LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word0);
    const uint32_t word1 =
        (static_cast<uint32_t>(Packet::Type::enter)) |
        ((uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle()) & 0x7F) << 1) |
        ((reinterpret_cast<uint32_t>(this_fn) & 0x00FFFFFF) << 8);
    LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word1);
    __set_PRIMASK(old_primask);
}

extern "C" LN_PROF_ATTR void __cyg_profile_func_exit(
    void *this_fn, [[maybe_unused]] void *call_site) {
    if (!started) {
        return;
    }
    if (xPortIsInsideInterrupt()) {
        return;
    }
    uint32_t old_primask = __get_PRIMASK();
    __disable_irq();
    const uint32_t cycle_count = DWT->CYCCNT;
    const uint32_t word0 =
        Packet::SYNC_BYTE | ((cycle_count - last_cycle_count) << 8);
    last_cycle_count = cycle_count;
    LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word0);
    const uint32_t word1 =
        (static_cast<uint32_t>(Packet::Type::exit)) |
        ((uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle()) & 0x7F) << 1) |
        ((reinterpret_cast<uint32_t>(this_fn) & 0x00FFFFFF) << 8);
    LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word1);
    __set_PRIMASK(old_primask);
}
