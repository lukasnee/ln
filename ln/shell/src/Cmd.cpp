/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/Cmd.hpp"
#include "ln/shell/CLI.hpp"

// TODO: make arrow up repeat buffer
#include <cstring>

namespace ln::shell {

ln::StaticForwardList<Cmd> Cmd::base_cmd_list = {};
ln::StaticForwardList<Cmd> Cmd::general_cmd_list = {};
ln::StaticForwardList<Cmd> Cmd::global_cmd_list = {};

Cmd::Cmd(Cfg cfg) : cfg{cfg} {

    auto &cmd_list = this->cfg.parent_cmd ? this->cfg.parent_cmd->children_cmd_list : this->cfg.cmd_list;
    cmd_list.push_front(*this);
}

static bool matches_any_token(std::string_view str_token, const char *str_tokens) {
    const char *str_this_token = str_tokens;
    for (const char *str_char_it = str_this_token; *str_char_it != '\0'; str_char_it++) {
        const bool it_at_last_char = (*(str_char_it + 1) == '\0');
        if (*str_char_it != ',' && !it_at_last_char) {
            continue;
        }
        const std::size_t this_token_length = str_char_it + (it_at_last_char ? 1 : 0) - str_this_token;
        if (str_token.size() == this_token_length &&
            0 == std::strncmp(str_token.data(), str_this_token, this_token_length)) {
            return true;
        }
        if (*str_char_it == ',') {
            str_this_token = str_char_it + 1;
        }
    }
    return false;
}

const Cmd *Cmd::find_cmd_by_name(ln::StaticForwardList<Cmd> cmd_list, std::string_view name) {
    for (const auto &cmd : cmd_list) {
        if (matches_any_token(name, cmd.cfg.name)) {
            return &cmd;
        }
    }
    return nullptr;
}

const Cmd *Cmd::find_child_cmd_by_name(std::string_view name) const {
    for (const auto &cmd : this->children_cmd_list) {
        if (matches_any_token(name, cmd.cfg.name)) {
            return &cmd;
        }
    }
    return nullptr;
}

void Cmd::print_short_help(CLI &cli, const std::size_t max_depth, std::size_t depth) const {
    for (std::size_t i = depth; i > 0; i--) {
        auto cmd = this;
        for (std::size_t j = 0; j < i; j++) {
            cmd = cmd->cfg.parent_cmd;
        }
        cli.print(cmd->cfg.name);
        cli.print(' ');
    }
    if (this->cfg.name) {
        cli.print(this->cfg.name);
    }
    if (this->cfg.usage) {
        cli.print(' ');
        cli.print(this->cfg.usage);
    }
    if (this->cfg.short_description) {
        cli.print(" - ");
        cli.print(this->cfg.short_description);
    }
    cli.print('\n');
    if (depth >= max_depth) {
        return;
    }
    for (const auto &cmd : this->children_cmd_list) {
        cmd.print_short_help(cli, max_depth, depth + 1);
    }
}

void Cmd::print_long_help(CLI &cli) const {
    this->print_short_help(cli, 0);
    if (this->cfg.long_description) {
        cli.print(this->cfg.long_description);
    }
    cli.print('\n');
}

Cmd help_cmd{Cmd::Cfg{.cmd_list = Cmd::base_cmd_list,
                      .name = "help,?",
                      .usage = "[all|[COMMAND...]]",
                      .short_description = "show command usage",
                      .fn = [](Cmd::Ctx ctx) {
                          if (ctx.args.size() == 0) {
                              for (const auto &cmd_list_ptr : ctx.cli.config.cmd_lists) {
                                  if (!cmd_list_ptr) {
                                      continue;
                                  }
                                  for (const auto &cmd : *cmd_list_ptr) {
                                      cmd.print_short_help(ctx.cli, 0);
                                  }
                              }
                              return Err::ok;
                          }
                          else if (ctx.args.size() == 1 && ctx.args[0] == "all"sv) {
                              for (const auto &cmd_list_ptr : ctx.cli.config.cmd_lists) {
                                  if (!cmd_list_ptr) {
                                      continue;
                                  }
                                  for (const auto &cmd : *cmd_list_ptr) {
                                      cmd.print_short_help(ctx.cli, 7);
                                  }
                              }
                              return Err::ok;
                          }
                          else if (ctx.args.size() >= 1) {
                              auto [cmd, _] = ctx.cli.find_cmd(ctx.args);
                              if (cmd) {
                                  cmd->print_long_help(ctx.cli);
                                  return Err::ok;
                              }
                          }
                          else {
                              return Err::badArg;
                          }
                          return Err::unknownCmd;
                      }}};

} // namespace ln::shell
