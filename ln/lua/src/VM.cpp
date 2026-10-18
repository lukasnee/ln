// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/lua/VM.hpp"

#include "ln/ln.h"

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <llimits.h>
}

void l_print(lua_State *L) {
    int n = lua_gettop(L);
    if (n > 0) { /* any result to be printed? */
        luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
        lua_getglobal(L, "print");
        lua_insert(L, 1);
        if (lua_pcall(L, n, 0, 0) != LUA_OK) {
            lua_writestringerror("error calling 'print' (%s)",
                                 lua_tostring(L, -1));
        }
    }
};

bool l_is_incomplete(lua_State *L, int status) {
    if (status == LUA_ERRSYNTAX) {
        size_t len;
        const char *msg = lua_tolstring(L, -1, &len);
        std::string_view error{msg, len};
        if (error.ends_with("<eof>")) {
            return true;
        }
    }
    return false;
};

namespace ln::lua {

VM::VM() {
    this->L = luaL_newstate();
    if (!this->L) {
        LN_PANIC();
        return;
    }
    luaL_openlibs(this->L);
}

VM::~VM() {
    if (this->L) {
        lua_close(this->L);
        this->L = nullptr;
    }
}

Interpreter::Err VM::interpret(std::string_view str) {
    // TODO: we could probably optimze by only tokenizing first word
    // and checking if it's a known command. If not, we can skip
    // directly to Lua execution.

    // 1) Try as expression: return <str>
    lua_pushlstring(this->L, "return ", strlen("return "));
    lua_pushlstring(this->L, str.data(), str.size());
    lua_concat(this->L, 2);
    int status = luaL_loadbuffer(this->L, lua_tostring(this->L, -1),
                                 lua_rawlen(this->L, -1), "=shell");
    int nresults = 0;
    if (status != LUA_OK) {
        if (l_is_incomplete(this->L, status)) {
            lua_pop(this->L, 2);
            return Err::incomplete;
        }
        lua_pop(this->L, 2);

        // 2) Try as statement/chunk: <str>
        status = luaL_loadbuffer(this->L, str.data(), str.size(), "=shell");
        if (status != LUA_OK) {
            if (l_is_incomplete(this->L, status)) {
                lua_pop(this->L, 1);
                return Err::incomplete;
            }
            puts(lua_tostring(this->L, -1));
            lua_pop(this->L, 1);
            return Err::compileError;
        }
        nresults = 0;
    }
    else {
        lua_remove(this->L, -2); // remove modified str
        nresults = LUA_MULTRET;
    }
    // 3) Run the chunk
    status = lua_pcall(this->L, 0, nresults, 0);
    if (status != LUA_OK) {
        puts(lua_tostring(this->L, -1));
        lua_pop(this->L, 1);
        return Err::runtimeError;
    }
    l_print(this->L);
    return Err::ok;
}

} // namespace ln::lua
