// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/ln.h"
#include "ln/port/exceptions.h"

extern "C"
{
#include "stm32f3xx.h"
}

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/chrono.h>

#include <cstdint>

namespace ln {

static uint32_t get_sp() {
    uint32_t sp = 0;
    __asm volatile("mov %0, sp \n" : "=r"(sp));
    return sp;
}

static uint32_t get_lr() {
    uint32_t lr = 0;
    __asm volatile("mov %0, lr \n" : "=r"(lr));
    return lr;
}

static uint32_t get_ipsr() {
    uint32_t ipsr = 0;
    __asm volatile("mrs %0, ipsr \n" : "=r"(ipsr));
    return ipsr;
}

void disable_irq() { __disable_irq(); }

void enable_irq() { __enable_irq(); }

/**
 * @brief Prints words in hexadecimal format.
 *
 * @param indent The string to print at the beginning of each line.
 * @param address The starting address to read from.
 * @param size The total words to print.
 * @param width The number of words to print per line.
 */
static void print_words_in_hex(std::string_view indent, uint32_t address,
                               size_t size, const size_t words_per_line) {
    while (size) {
        const size_t line_words = std::min(size, words_per_line);
        fmt::print(
            "{}{:08X}\n", indent,
            fmt::join(std::span<uint32_t>(reinterpret_cast<uint32_t *>(address),
                                          line_words),
                      " "));
        address += line_words * sizeof(uint32_t);
        size -= line_words;
    }
}

struct ExceptionStackFrameBasic {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t return_address;
    uint32_t retpsr;
};

struct ExceptionStackFrameExtended {
    ExceptionStackFrameBasic basic;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t s12;
    uint32_t s13;
    uint32_t s14;
    uint32_t s15;
    uint32_t fpscr;
    uint32_t vpr;
};

union ExceptionStackFrame {
    ExceptionStackFrameBasic basic;
    ExceptionStackFrameExtended extended;
};

static void hardfault_handler(void *exception_stack_frame) {
    ExceptionStackFrame *frame =
        reinterpret_cast<ExceptionStackFrame *>(exception_stack_frame);
    const auto exc_return = get_lr();
    enum class SPSEL {
        main,
        process
    };
    const SPSEL spsel = exc_return & (1 << 2) ? SPSEL::process : SPSEL::main;
    enum Mode {
        thread,
        handler
    };
    const Mode mode = exc_return & (1 << 3) ? Mode::thread : Mode::handler;
    enum class FType {
        basic,
        extended
    };
    const FType ftype = exc_return & (1 << 4) ? FType::basic : FType::extended;

    fmt::print("\n# system hardfault!\n");
    fmt::print("uptime: {}\n", ln::get_uptime_ms());
    const uint32_t sp =
        reinterpret_cast<uint32_t>(frame) +
        (ftype == FType::basic ? sizeof(ExceptionStackFrameBasic)
                               : sizeof(ExceptionStackFrameExtended));
    fmt::print("SP: {:08X} ({})\n", sp, spsel == SPSEL::main ? "MSP" : "PSP");
    fmt::print("SPSEL: {}\n", spsel == SPSEL::main ? "main" : "process");
    fmt::print("mode: {}\n", mode == Mode::thread ? "thread" : "handler");

    const std::array<std::pair<uint32_t, const char *>, 22>
        CFSR_bitmask_to_str = {{{SCB_CFSR_USGFAULTSR_Msk, "USGFAULTSR"},
                                {SCB_CFSR_BUSFAULTSR_Msk, "BUSFAULTSR"},
                                {SCB_CFSR_MEMFAULTSR_Msk, "MEMFAULTSR"},
                                {SCB_CFSR_MMARVALID_Msk, "MMARVALID"},
                                {SCB_CFSR_MLSPERR_Msk, "MLSPERR"},
                                {SCB_CFSR_MSTKERR_Msk, "MSTKERR"},
                                {SCB_CFSR_MUNSTKERR_Msk, "MUNSTKERR"},
                                {SCB_CFSR_DACCVIOL_Msk, "DACCVIOL"},
                                {SCB_CFSR_IACCVIOL_Msk, "IACCVIOL"},
                                {SCB_CFSR_BFARVALID_Msk, "BFARVALID"},
                                {SCB_CFSR_LSPERR_Msk, "LSPERR"},
                                {SCB_CFSR_STKERR_Msk, "STKERR"},
                                {SCB_CFSR_UNSTKERR_Msk, "UNSTKERR"},
                                {SCB_CFSR_IMPRECISERR_Msk, "IMPRECISERR"},
                                {SCB_CFSR_PRECISERR_Msk, "PRECISERR"},
                                {SCB_CFSR_IBUSERR_Msk, "IBUSERR"},
                                {SCB_CFSR_DIVBYZERO_Msk, "DIVBYZERO"},
                                {SCB_CFSR_UNALIGNED_Msk, "UNALIGNED"},
                                {SCB_CFSR_NOCP_Msk, "NOCP"},
                                {SCB_CFSR_INVPC_Msk, "INVPC"},
                                {SCB_CFSR_INVSTATE_Msk, "INVSTATE"},
                                {SCB_CFSR_UNDEFINSTR_Msk, "UNDEFINSTR"}}};
    fmt::print("CFSR: {:08X} (", static_cast<uint32_t>(SCB->CFSR));
    bool first = true;
    for (auto &[bitmask, name] : CFSR_bitmask_to_str) {
        if (SCB->CFSR & bitmask) {
            if (!first) {
                fmt::print("|");
            }
            fmt::print("{}", reinterpret_cast<const char *>(name));
            first = false;
        }
    }
    fmt::print(")\n");
    fmt::print(
        "HFSR: {:08X}\n"
        "DFSR: {:08X}\n"
        "MMFAR: {:08X}\n"
        "BFAR: {:08X}\n"
        "AFSR: {:08X}\n",
        static_cast<uint32_t>(SCB->HFSR), static_cast<uint32_t>(SCB->DFSR),
        static_cast<uint32_t>(SCB->MMFAR), static_cast<uint32_t>(SCB->BFAR),
        static_cast<uint32_t>(SCB->AFSR));

    fmt::print("exception stack frame:\n"
               "  R0: {:08X}\n"
               "  R1: {:08X}\n"
               "  R2: {:08X}\n"
               "  R3: {:08X}\n"
               "  R12: {:08X}\n"
               "  LR: {:08X}\n"
               "  return_address: {:08X}\n"
               "  retpsr: {:08X}\n",
               frame->basic.r0, frame->basic.r1, frame->basic.r2,
               frame->basic.r3, frame->basic.r12, frame->basic.lr,
               frame->basic.return_address, frame->basic.retpsr);
    if (ftype == FType::extended) {
        fmt::print("  S0: {:08X}\n"
                   "  S1: {:08X}\n"
                   "  S2: {:08X}\n"
                   "  S3: {:08X}\n"
                   "  S4: {:08X}\n"
                   "  S5: {:08X}\n"
                   "  S6: {:08X}\n"
                   "  S7: {:08X}\n"
                   "  S8: {:08X}\n"
                   "  S9: {:08X}\n"
                   "  S10: {:08X}\n"
                   "  S11: {:08X}\n"
                   "  S12: {:08X}\n"
                   "  S13: {:08X}\n"
                   "  S14: {:08X}\n"
                   "  S15: {:08X}\n"
                   "  FPSCR: {:08X}\n"
                   "  VPR: {:08X}\n",
                   frame->extended.s0, frame->extended.s1, frame->extended.s2,
                   frame->extended.s3, frame->extended.s4, frame->extended.s5,
                   frame->extended.s6, frame->extended.s7, frame->extended.s8,
                   frame->extended.s9, frame->extended.s10, frame->extended.s11,
                   frame->extended.s12, frame->extended.s13,
                   frame->extended.s14, frame->extended.s15,
                   frame->extended.fpscr, frame->extended.vpr);
    }
    fmt::print("stack dump:\n");
    const size_t words_to_dump = 64;
    print_words_in_hex("  ", sp, words_to_dump, 4);
    reset();
}

void panic_port(const char *file, int line, const char *message) {
    const auto sp = get_sp();
    const auto ipsr = get_ipsr();
    fmt::print("\n# panic!\n");
    fmt::print("message: {}\n", (message ? message : "null"));
    fmt::print("location: {}:{}\n", file, line);
    fmt::print("uptime: {} ms\n", get_uptime_ms().count());
    fmt::print("SP: {:08X}\n", sp);
    fmt::print("IPSR: {} ({} mode)\n", ipsr,
               (ipsr == 0 ? "thread" : "handler/interrupt"));
    fmt::print("stack dump:\n");
    const size_t words_to_dump = 64;
    print_words_in_hex("  ", sp, words_to_dump, 4);
}

void reset() {
    __NVIC_SystemReset();
    while (true) {
    }
}

} // namespace ln

extern "C" void _ln_hardfault_handler(void *frame) {
    ln::hardfault_handler(frame);
}

extern "C" void ln_FreeRTOS_portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() {
    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->CR1 = 0;
    const size_t timer_frequency = 10000;
    TIM2->PSC =
        static_cast<std::uint16_t>((SystemCoreClock / timer_frequency) - 1);
    TIM2->ARR = 0xFFFFFFFF;
    TIM2->CNT = 0;
    TIM2->EGR = TIM_EGR_UG;

    TIM2->CR1 |= TIM_CR1_CEN;
}

extern "C" uint32_t ln_FreeRTOS_portGET_RUN_TIME_COUNTER_VALUE() {
    return TIM2->CNT;
}
