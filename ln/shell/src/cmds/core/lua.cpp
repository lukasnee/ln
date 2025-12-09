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
#include <llimits.h>
}

const char *progname = nullptr;
static lua_State *L = nullptr;

extern "C"
{
    /*
    ** Prints an error message, adding the program name in front of it
    ** (if present)
    */
    static void l_message(const char *pname, const char *msg) {
        if (pname)
            lua_writestringerror("%s: ", pname);
        lua_writestringerror("%s\n", msg);
    }

    /*
    ** Check whether 'status' is not OK and, if so, prints the error
    ** message on the top of the stack.
    */
    static int report(lua_State *L, int status) {
        if (status != LUA_OK) {
            const char *msg = lua_tostring(L, -1);
            if (msg == NULL)
                msg = "(error message not a string)";
            l_message(progname, msg);
            lua_pop(L, 1); /* remove message */
        }
        return status;
    }

    /*
     ** Message handler used to run all chunks
     */
    static int msghandler(lua_State *L) {
        const char *msg = lua_tostring(L, 1);
        if (msg == NULL) {                           /* is error object not a string? */
            if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
                lua_type(L, -1) == LUA_TSTRING)      /* that produces a string? */
                return 1;                            /* that is the message */
            else
                msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
        }
        luaL_traceback(L, L, msg, 1); /* append a standard traceback */
        return 1;                     /* return the traceback */
    }

    /*
     ** Utility function to dump the Lua stack for debugging
     */
    static void dump_lua_stack(lua_State *L, const char *label) {
        int top = lua_gettop(L);
        printf("=== Lua Stack Dump: %s ===\n", label ? label : "");
        printf("Stack size: %d\n", top);

        for (int i = 1; i <= top; i++) {
            int type = lua_type(L, i);
            printf("  [%d] %s: ", i, lua_typename(L, type));

            switch (type) {
            case LUA_TNIL:
                printf("nil");
                break;
            case LUA_TBOOLEAN:
                printf(lua_toboolean(L, i) ? "true" : "false");
                break;
            case LUA_TNUMBER:
                if (lua_isinteger(L, i)) {
                    printf("%lld", (long long)lua_tointeger(L, i));
                }
                else {
                    printf("%f", lua_tonumber(L, i));
                }
                break;
            case LUA_TSTRING:
                printf("\"%s\"", lua_tostring(L, i));
                break;
            case LUA_TTABLE:
                printf("table: %p", lua_topointer(L, i));
                break;
            case LUA_TFUNCTION:
                printf("function: %p", lua_topointer(L, i));
                break;
            default:
                printf("%p", lua_topointer(L, i));
                break;
            }
            printf("\n");
        }
        printf("========================\n");
    }

    /*
    ** Interface to 'lua_pcall', which sets appropriate message function
    ** and C-signal handler. Used to run all chunks.
    */
    static int docall(lua_State *L, int narg, int nres) {
        int status;
        int base = lua_gettop(L) - narg;  /* function index */
        lua_pushcfunction(L, msghandler); /* push message handler */
        lua_insert(L, base);              /* put it under function and args */
        // globalL = L;                      /* to be available to 'laction' */
        // setsignal(SIGINT, laction);       /* set C-signal handler */
        status = lua_pcall(L, narg, nres, base);
        // setsignal(SIGINT, SIG_DFL); /* reset C-signal handler */
        lua_remove(L, base); /* remove message handler from the stack */
        return status;
    }

    /*
    ** Prints (calling the Lua 'print' function) any values on the stack
    */
    static void l_print(lua_State *L) {
        int n = lua_gettop(L);
        if (n > 0) { /* any result to be printed? */
            luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
            lua_getglobal(L, "print");
            lua_insert(L, 1);
            if (lua_pcall(L, n, 0, 0) != LUA_OK)
                l_message(progname, lua_pushfstring(L, "error calling 'print' (%s)", lua_tostring(L, -1)));
        }
    }
}

namespace ln::shell {

Cmd cmd_lua{Cmd::Cfg{
    .cmd_list = Cmd::general_cmd_list, .name = "lua", .short_description = "runs lua code", .fn = [](Cmd::Ctx ctx) {
        if (!L) {
            L = luaL_newstate();
            luaL_openlibs(L);
        }
        std::string_view code_sv{ctx.args.front().cbegin(), ctx.args.back().cend()};
        if (luaL_loadbuffer(L, code_sv.data(), code_sv.size(), code_sv.data()) == LUA_OK) {
            const auto status = docall(L, 0, LUA_MULTRET);
            if (status == LUA_OK) {
                // If it was executed successfuly we
                // remove the code from the stack
                // lua_pop(L, lua_gettop(L));
                l_print(L);
            }
            else {
                report(L, status);
            }
            lua_settop(L, 0); // clear the stack
        }
        return Err::ok;
    }}};

Cmd cmd_luareset{Cmd::Cfg{.cmd_list = Cmd::general_cmd_list,
                          .name = "luareset",
                          .short_description = "resets lua state",
                          .fn = [](Cmd::Ctx ctx) {
                              if (L) {
                                  lua_close(L);
                                  L = nullptr;
                              }
                              return Err::ok;
                          }}};

Cmd cmd_luastack{Cmd::Cfg{.cmd_list = Cmd::general_cmd_list,
                          .name = "luastack",
                          .short_description = "dumps current lua stack state",
                          .fn = [](Cmd::Ctx ctx) {
                              if (!L) {
                                  printf("No Lua state initialized\n");
                                  return Err::ok;
                              }

                              dump_lua_stack(L, "Current State");

                              // Also show memory usage
                              int memory_kb = lua_gc(L, LUA_GCCOUNT, 0);
                              int memory_bytes = lua_gc(L, LUA_GCCOUNTB, 0);
                              printf("Lua memory usage: %d KB + %d bytes\n", memory_kb, memory_bytes);

                              return Err::ok;
                          }}};

#include "FreeRTOS/Addons/Clock.hpp"

#include <charconv>

Cmd cmd_luatest{
    Cmd::Cfg{.cmd_list = Cmd::general_cmd_list,
             .name = "luatest",
             .usage = "<fib_n>",
             .short_description = "test lua with fibonachi sequence using recursive index metamethod and memoisation ",
             .fn = [](Cmd::Ctx ctx) {
                 if (ctx.args.size() != 1) {
                     return Err::badArg;
                 }
                 int n = 0;
                 // TODO: write arithemtic parser helper using std::from_chars: std::optional<int> opt_n =
                 // Args::parse_int(ctx.args[0]);
                 auto result = std::from_chars(ctx.args[0].data(), ctx.args[0].data() + ctx.args[0].size(), n);
                 if (result.ec != std::errc{}) {
                     return Err::badArg;
                 }

                 if (!L) {
                     L = luaL_newstate();
                     luaL_openlibs(L);
                 }
                 lua_getglobal(L, "fib");
                 bool is_fib_function = lua_isfunction(L, -1);
                 lua_pop(L, 1);
                 if (!is_fib_function) {
                     const char *lua_code = R"(
local fib_mt = {
    __index = function(self, n)
        if n < 2 then return n end
        self[n] = self[n - 2] + self[n - 1]
        return self[n]
    end
}
fib = setmetatable({}, fib_mt)

function get_fib(n)
    return fib[n]
end
)";
                     auto st = luaL_dostring(L, lua_code);
                     if (st != LUA_OK) {
                         report(L, st);
                         return Err::fail;
                     }
                 }
                 lua_getglobal(L, "get_fib");
                 lua_pushinteger(L, n);
                 auto ts_start = FreeRTOS::Addons::Clock::now();
                 if (docall(L, 1, 1) != LUA_OK) {
                     report(L, LUA_ERRRUN);
                     return Err::fail;
                 }
                 auto ts_end = FreeRTOS::Addons::Clock::now();
                 lua_Integer result_n = lua_tointeger(L, -1);
                 lua_pop(L, 1); // remove result from stack
                 printf("get_fib[%d] = %lld (elapsed: %lld ms)\n", n, static_cast<long long>(result_n),
                        std::chrono::duration_cast<std::chrono::milliseconds>(ts_end - ts_start).count());
                 return Err::ok;
             }}};

} // namespace ln::shell
