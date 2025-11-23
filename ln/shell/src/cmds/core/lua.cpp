/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/CLI.hpp"

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace ln::shell {

Cmd lua("lua", "runs lua code", [](Cmd::Ctx ctx) -> Err {
    (void)ctx;
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    const char *code = "print('Hello, World')";
    if (luaL_loadstring(L, code) == LUA_OK) {
        if (lua_pcall(L, 0, 0, 0) == LUA_OK) {
            // If it was executed successfuly we
            // remove the code from the stack
            lua_pop(L, lua_gettop(L));
        }
    }
    lua_close(L);
    return Err::ok;
});

} // namespace ln::shell