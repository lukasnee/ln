// Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/prof/prof.h"

#ifndef LN_PROF_ITM_PORT
#define LN_PROF_ITM_PORT 0
#endif

#include "ln/port/itm.h"

#include "FreeRTOS.h"
#include "task.h"

#include <bitset>
#include <cstdint>

struct Packet {

    enum class Id : uint32_t {
        profile = 0xA5,
        context_name = 0xA6,
    };

    struct Profile {
        enum class Type : uint32_t {
            enter = 0,
            exit = 1,
        };
    };

#pragma pack(push, 1)
    union {
        struct {
            uint32_t sync_byte : 8;
            uint32_t cycle_cnt_delta : 24;
            uint32_t type : 1;
            uint32_t context_id : 7;
            uint32_t fn : 24; // MSB shall be provided by the other packet
        };
        uint32_t as_u32[2];
    };
#pragma pack(pop)
};

volatile bool started = false;
static uint32_t last_cycle_count = 0;
static uint32_t next_context_id = 1;
static std::bitset<128> announced_contexts;
static uint32_t last_announce_reset = 0;

static void send_task_name(uint32_t context_id, const char *name) {
    LN_ITM_SEND_WORD(
        LN_PROF_ITM_PORT,
        static_cast<uint32_t>(Packet::Id::context_name) | (context_id << 8) |
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

static inline uint32_t current_context_id() {
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
        announced_contexts.reset();
    }
    if (!announced_contexts.test(id)) {
        announced_contexts.set(id);
        send_task_name(id, pcTaskGetName(task));
    }
    return id;
}

bool ln_prof_start() {

    // Enable trace (required for ITM and DWT)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // Reset and enable cycle counter (used by ln/prof for timestamps)
    DWT->CYCCNT = 0;
    last_cycle_count = 0;
    last_announce_reset = 0;
    announced_contexts.reset();
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
    const uint32_t context_id = current_context_id();
    const uint32_t cycle_count = DWT->CYCCNT;
    const uint32_t word0 = static_cast<uint32_t>(Packet::Id::profile) |
                           ((cycle_count - last_cycle_count) << 8);
    last_cycle_count = cycle_count;
    LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word0);
    const uint32_t word1 =
        (static_cast<uint32_t>(Packet::Profile::Type::enter)) |
        (context_id << 1) |
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
    const uint32_t context_id = current_context_id();
    const uint32_t cycle_count = DWT->CYCCNT;
    const uint32_t word0 = static_cast<uint32_t>(Packet::Id::profile) |
                           ((cycle_count - last_cycle_count) << 8);
    last_cycle_count = cycle_count;
    LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word0);
    const uint32_t word1 =
        (static_cast<uint32_t>(Packet::Profile::Type::exit)) |
        (context_id << 1) |
        ((reinterpret_cast<uint32_t>(this_fn) & 0x00FFFFFF) << 8);
    LN_ITM_SEND_WORD(LN_PROF_ITM_PORT, word1);
    __set_PRIMASK(old_primask);
}
