// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/shell/CLI.hpp"

namespace ln::shell {

Cmd cmd_clear{Cmd::Cfg{.cmd_list = Cmd::get_general_cmd_list(),
                       .name = "clear",
                       .short_description = "clear screen",
                       .fn = [](Cmd::Ctx ctx) {
                           ctx.cli.clear_screen();
                           return Err::ok;
                       }}};

} // namespace ln::shell
