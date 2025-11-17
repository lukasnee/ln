/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */



#include "ln/shell/generic/cmds.hpp"

namespace ln::shell::generic::cmds {

Err on_off_command_parser(std::function<bool(bool)> on_off_fn, const char *strOnOffControlName, Cmd::Ctx ctx) {
    using namespace std::literals::string_view_literals;
    if (ctx.args.size() != 1) {
        ctx.cli.print("error: no arg\n");
        return Err::badArg;
    }
    if (ctx.args[0] == "on"sv || ctx.args[0] == "1"sv || ctx.args[0] == "true"sv) {
        if (on_off_fn(true)) {
            return Err::ok;
        }
        ctx.cli.printf("error: failed to turn on %s\n", strOnOffControlName);
        return Err::fail;
    }
    if (ctx.args[0] == "off"sv || ctx.args[0] == "0"sv || ctx.args[0] == "false"sv) {
        if (on_off_fn(false)) {
            return Err::ok;
        }
        ctx.cli.printf("error: failed to turn off %s\n", strOnOffControlName);
        return Err::fail;
    }
    ctx.cli.print("error: unexpected arg\n");
    return Err::badArg;
};

Err on_off_command_parser(bool &onOffControl, const char *strOnOffControlName, Cmd::Ctx ctx) {
    return on_off_command_parser(
        [&](bool state) {
            onOffControl = state;
            return true;
        },
        strOnOffControlName, ctx);
};

} // namespace ln::shell::generic::cmds