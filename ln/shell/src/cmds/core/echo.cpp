// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/shell/CLI.hpp"

#include <fmt/ranges.h>

namespace ln::shell {

Cmd cmd_echo{Cmd::Cfg{.cmd_list = Cmd::get_general_cmd_list(),
                      .name = "echo",
                      .short_description = "echos typed content",
                      .fn = [](Cmd::Ctx ctx) {
                          if (ctx.args.empty()) {
                              ctx.cli.print('\n');
                              return Err::ok;
                          }
                          ctx.cli.print("{}\n", fmt::join(ctx.args, " "));
                          return Err::ok;
                      }}};

} // namespace ln::shell
