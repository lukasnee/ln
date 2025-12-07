/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/CLI.hpp"

namespace ln::shell {

Cmd clear_cmd{Cmd::Cfg{
    .cmd_list = Cmd::general_cmd_list, .name = "clear,c", .short_description = "clear screen", .fn = [](Cmd::Ctx ctx) {
        std::size_t i = 0x30;
        while (i--) {
            ctx.cli.print('\n');
        }
        return Err::ok;
    }}};

} // namespace ln::shell
