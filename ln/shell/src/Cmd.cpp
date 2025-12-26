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

Cmd::Cmd(Cfg cfg) : cfg{std::move(cfg)} {

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

void Cmd::print_short_help(CLI &cli, std::size_t max_depth, std::size_t depth) const {
    const auto top_level_call = depth == 0;
    if (top_level_call) {
        /* Top-level call of this function considers max_depth and depth
         * arguments relative to the current command. We need to switch to
         * absolute values.
         */
        const auto curr_cmd_depth = this->resolve_cmd_depth();
        max_depth += curr_cmd_depth;
        depth += curr_cmd_depth;
    }
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
    else {
        for (const auto &arg : this->cfg.argp_cfg.positional_args) {
            cli.print(' ');
            cli.print('<');
            cli.print(arg.name);
            cli.print(':');
            cli.print(Arg::to_string(arg.type));
            cli.print('>');
        }
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

void Cmd::print_args(CLI &cli) const {
    if (this->cfg.argp_cfg.positional_args.empty()) {
        return;
    }
    cli.print("Positional arguments:\n");
    for (const auto &arg : this->cfg.argp_cfg.positional_args) {
        cli.print("  ");
        cli.print(arg.name);
        cli.print(" : ");
        cli.print(Arg::to_string(arg.type));
        if (!arg.description.empty()) {
            cli.print(" - ");
            cli.print(arg.description);
        }
        cli.print('\n');
    }
}

std::size_t Cmd::resolve_cmd_depth() const {
    std::size_t depth = 0;
    const Cmd *cmd_it = this;
    while (cmd_it->cfg.parent_cmd) {
        depth++;
        cmd_it = cmd_it->cfg.parent_cmd;
    }
    return depth;
}

void Cmd::print_long_help(CLI &cli, std::size_t max_depth, std::size_t depth) const {
    const auto top_level_call = depth == 0;
    if (top_level_call) {
        /* Top-level call of this function considers max_depth and depth
         * arguments relative to the current command. We need to switch to
         * absolute values.
         */
        const auto curr_cmd_depth = this->resolve_cmd_depth();
        max_depth += curr_cmd_depth;
        depth += curr_cmd_depth;
    }
    this->print_short_help(cli, 0);
    this->print_args(cli);
    if (top_level_call && this->cfg.long_description) {
        cli.print(this->cfg.long_description);
        cli.print('\n');
    }
    if (depth >= max_depth) {
        return;
    }
    for (const auto &cmd : this->children_cmd_list) {
        cmd.print_short_help(cli, max_depth, depth + 1);
    }
}

Cmd cmd_help{Cmd::Cfg{.cmd_list = Cmd::base_cmd_list,
                      .name = "help,?",
                      .usage = "[<cmd_name:str>][--all]",
                      .short_description = "show help information about commands",
                      .fn = [](Cmd::Ctx ctx) {
                          const std::size_t depth_of_all = 7;
                          // TODO: proper abstraction to parse optional args
                          using namespace std::literals::string_view_literals;
                          const bool all = ctx.args.back() == "--all"sv;
                          const auto cmd_args = all ? ctx.args.subspan(0, ctx.args.size() - 1) : ctx.args;
                          if (cmd_args.empty()) {
                              for (const auto &cmd_list_ptr : ctx.cli.config.cmd_lists) {
                                  if (!cmd_list_ptr) {
                                      continue;
                                  }
                                  for (const auto &cmd : *cmd_list_ptr) {
                                      cmd.print_short_help(ctx.cli, all ? depth_of_all : 0);
                                  }
                              }
                              return Err::ok;
                          }
                          if (cmd_args.empty()) {
                              return Err::badArg;
                          }
                          auto [cmd, _] = ctx.cli.find_cmd(ctx.args);
                          if (cmd) {
                              cmd->print_long_help(ctx.cli, all ? depth_of_all : 1);
                              return Err::ok;
                          }
                          return Err::unknownCmd;
                      }}};

} // namespace ln::shell
