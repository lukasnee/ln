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

Cmd *Cmd::global_command_list = nullptr;

void Cmd::link_to(Cmd *&parent_cmd) {
    if (!parent_cmd) {
        parent_cmd = this;
    }
    else {
        Cmd *next = parent_cmd;
        while (next->next) {
            next = next->next;
        }
        next->next = this;
    }
}

Cmd::Cmd(const char *name, const char *usage, const char *description, Cmd::Function function,
         std::function<void()> ctor_cb)
    : name(name), usage(usage), description(description), function(function) {
    this->link_to(Cmd::global_command_list);
    if (ctor_cb) {
        ctor_cb();
    }
}

Cmd::Cmd(Cmd &parent_cmd, const char *name, const char *usage, const char *description, Cmd::Function function,
         std::function<void()> ctor_cb)
    : name(name), usage(usage), description(description), function(function) {
    this->link_to(parent_cmd.subcmd);
    if (ctor_cb) {
        ctor_cb();
    }
}

Cmd::Cmd(const char *name, const char *description, Function function)
    : name(name), usage(nullptr), description(description), function(function) {
    this->link_to(Cmd::global_command_list);
}

Cmd::Cmd(const char *name, Cmd::Function function)
    : name(name), usage(nullptr), description(nullptr), function(function) {
    this->link_to(Cmd::global_command_list);
}

bool Cmd::match_token(const char *str_tokens, std::string_view str_token) {
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

const Cmd *Cmd::find_neighbour_cmd(std::string_view name) const {
    for (const Cmd *next = this; next != nullptr; next = next->next) {
        if (Cmd::match_token(next->name, name)) {
            return next;
        }
    }
    return nullptr;
}

const Cmd *Cmd::find_subcmd(std::string_view name) const {
    for (const Cmd *next = this->subcmd; next != nullptr; next = next->next) {
        if (Cmd::match_token(next->name, name)) {
            return next;
        }
    }

    return nullptr;
}

void Cmd::print_help(CLI &cli, bool recurse, const std::size_t max_depth, std::size_t depth, std::size_t indent) const {
    constexpr int cmd_column_width = 40;
    for (const Cmd *cmd_it = this; cmd_it != nullptr; cmd_it = cmd_it->next) {
        if (indent >= 3) {
            cli.print(' ', indent - 3);
            // cli.print("|\n");
            // cli.print(' ', indent - 3);
            cli.print("`- ");
        }
        int chars_printed = 0;
        if (cmd_it->name) {
            if (cmd_it->usage) {
                chars_printed = cli.printf("%s %s ", cmd_it->name, cmd_it->usage);
            }
            else {
                chars_printed = cli.printf("%s  ", cmd_it->name);
            }
        }
        if (chars_printed > 0) {
            if (chars_printed < cmd_column_width) {
                cli.print(' ', cmd_column_width - chars_printed - indent);
            }
            if (cmd_it->description) {
                chars_printed = cli.print(cmd_it->description);
            }
            cli.print('\n');
        }
        if (recurse && depth < max_depth && cmd_it->subcmd) {
            cmd_it->subcmd->print_help(cli, recurse, max_depth, depth + 1, indent + strlen(cmd_it->name) + sizeof(' '));
        }
        if (depth == 0) {
            break;
        }
    }
}

Cmd Cmd::help_cmd = Cmd("help,?", "[all|[COMMAND...]]", "show command usage", [](Ctx ctx) -> Err {
    if (ctx.args.size() == 0) {
        for (const Cmd *cmd_it = Cmd::global_command_list; cmd_it; cmd_it = cmd_it->next) {
            cmd_it->print_help(ctx.cli, false, 0);
        }
    }
    else if (ctx.args.size() == 1 && ctx.args[0] == "all"sv) {
        for (const Cmd *cmd_it = Cmd::global_command_list; cmd_it; cmd_it = cmd_it->next) {
            cmd_it->print_help(ctx.cli, true, 7);
        }
    }
    else if (ctx.args.size() >= 1) {
        auto [cmd, _] = ctx.cli.find_cmd(ctx.args.subspan(1));
        if (cmd) {
            cmd->print_help(ctx.cli, true, 1);
        }
    }
    else {
        return Err::badArg;
    }
    return Err::ok;
});

} // namespace ln::shell
