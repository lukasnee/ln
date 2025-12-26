/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "ln/shell/Cmd.hpp"

namespace ln::shell::generic::cmds {
// TODO: make better API for on_off_command "overriding". Now its a bit clunky.

static constexpr const char *on_off_command_usage = "<on|off>";

Err on_off_command_parser(std::function<bool(bool)> on_off_fn, const char *ctrl_name, Cmd::Ctx ctx);
Err on_off_command_parser(bool &dst_state, const char *ctrl_name, Cmd::Ctx ctx);

} // namespace ln::shell::generic::cmds
