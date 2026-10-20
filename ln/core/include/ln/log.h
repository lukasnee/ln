// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

// Logging macro API. Always available, functional only if ln::logger is built
// in (LN_LOGGER is defined), otherwise all macros expand to nothing.

#ifdef LN_LOGGER

#include "ln/logger/logger.h"

#else

#define LN_LOG_EXTERN(_name)
#define LN_LOG_SCOPE(_name)
#define LN_LOG_MODULE_DEFINITION(_name, _level)
#define LN_LOG_MODULE(_name, _level)
#define LN_LOG_MODULE_CLASS_MEMBER(_name, _level)

#define LN_LOG(_level, ...)
#define LN_LOG_DBG(...)
#define LN_LOG_INF(...)
#define LN_LOG_WRN(...)
#define LN_LOG_ERR(...)
#define LN_LOG_CRT(...)

#define LN_LOG_FLUSH()

#endif // LN_LOGGER
