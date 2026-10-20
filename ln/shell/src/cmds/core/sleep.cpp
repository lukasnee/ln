// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/shell/CLI.hpp"

namespace ln::shell {

static constexpr std::array<Arg, 1> cmd_sleep_args{
    {Arg{.role = Arg::Role::positional,
         .name = "ms",
         .type = Arg::Type::num,
         .description = "Milliseconds to sleep"}}};

Cmd cmd_sleep{Cmd::Cfg{.cmd_list = Cmd::get_general_cmd_list(),
                       .name = "sleep",
                       .usage = "<ms>",
                       .args = cmd_sleep_args,
                       .short_description = "sleep for a given duration",
                       .fn = [](Cmd::Ctx ctx) {
                           int ms{};
                           auto [ptr, ec] = std::from_chars(
                               ctx.args[0].data(),
                               ctx.args[0].data() + ctx.args[0].size(), ms);
                           if (ec != std::errc()) {
                               return Err::badArg;
                           }
                           ln::sleep(std::chrono::milliseconds(ms));
                           return Err::ok;
                       }}};

} // namespace ln::shell
