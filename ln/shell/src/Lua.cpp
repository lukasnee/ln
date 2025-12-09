/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/Lua.hpp"

namespace ln::shell {

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

LuaTask &LuaTask::get_instance() {
    static LuaTask instance;
    return instance;
}

LuaTask::LuaTask() : FreeRTOS::Task(tskIDLE_PRIORITY + 1, (12 * 1024 / sizeof(StackType_t)), "lua") {}

void LuaTask::taskFunction() {

}
} // namespace ln::shell