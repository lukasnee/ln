// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "stm32f3xx_hal.h"

#define LN_ITM_IS_ENABLED() ((ITM->TCR & ITM_TCR_ITMENA_Msk) != 0UL)
#define LN_ITM_IS_PORT_ENABLED(port) ((ITM->TER & (1UL << (port))) != 0UL)
#define LN_ITM_IS_PORT_READY(port)                                             \
    (LN_ITM_IS_ENABLED() && LN_ITM_IS_PORT_ENABLED((port)))

#define LN_ITM_IS_FIFO_READY(port) (ITM->PORT[(port)].u32 != 0UL)

#define LN_ITM_SEND_WORD(port, value)                                          \
    while (!LN_ITM_IS_FIFO_READY((port))) {                                    \
        __NOP();                                                               \
    }                                                                          \
    ITM->PORT[(port)].u32 = (value)
