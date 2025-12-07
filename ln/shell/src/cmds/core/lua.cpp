/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/CLI.hpp"

#include <string_view>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace ln::shell {

Cmd cmd_lua{Cmd::Cfg{
    .cmd_list = Cmd::general_cmd_list, .name = "lua", .short_description = "runs lua code", .fn = [](Cmd::Ctx ctx) {
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);
        std::string_view code_sv{ctx.args.front().cbegin(), ctx.args.back().cend()};
        if (luaL_loadbuffer(L, code_sv.data(), code_sv.size(), code_sv.data()) == LUA_OK) {
            if (lua_pcall(L, 0, 0, 0) == LUA_OK) {
                // If it was executed successfuly we
                // remove the code from the stack
                lua_pop(L, lua_gettop(L));
            }
        }
        lua_close(L);
        return Err::ok;
    }}};

} // namespace ln::shell