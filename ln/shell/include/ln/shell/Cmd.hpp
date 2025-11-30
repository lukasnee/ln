/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "ln/StaticForwardList.hpp"

#include <functional>
#include <limits>
#include <cstdint>
#include <span>
#include <string_view>

namespace ln::shell {

enum Err : std::int8_t {
    unknown = std::numeric_limits<std::int8_t>::min(),
    unexpected = unknown + 1,
    unknownCmd = -3,
    badArg = -2,
    fail = -1,
    ok = 0
};

class CLI;

class Cmd : public ln::StaticForwardListNode<Cmd> {
public:
    struct Ctx {
        CLI &cli;
        std::span<const std::string_view> args;
    };

    using Function = std::function<Err(Ctx)>;
    using CtorCallback = std::function<void()>;

    Cmd(const char *name, const char *usage, const char *description, Function function,
        CtorCallback ctor_cb = nullptr);
    Cmd(Cmd &parent, const char *name, const char *usage, const char *description, Function function,
        CtorCallback ctor_cb = nullptr);
    Cmd(const char *name, const char *description, Function function);
    Cmd(const char *name, Function function);

    static const Cmd *find_cmd_by_name(ln::StaticForwardList<Cmd> cmd_list, std::string_view name);
    const Cmd *find_subcmd_by_name(std::string_view name) const;

    static bool match_token(const char *str_tokens, std::string_view str_token);

    const char *name = nullptr;
    const char *usage = nullptr;
    const char *description = nullptr;
    const Function function = nullptr;

    void print_help(CLI &cli, bool recurse, const std::size_t max_depth = 1, std::size_t depth = 0,
                    std::size_t indent = 0) const;

protected:
    ln::StaticForwardList<Cmd> subcmd_list;

private:
    friend CLI;

    static ln::StaticForwardList<Cmd> global_cmd_list;

    static Cmd help_cmd;
};

} // namespace ln::shell
