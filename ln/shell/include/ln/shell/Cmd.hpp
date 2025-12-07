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

    using Fn = std::function<Err(Ctx)>;

    struct Cfg {
        /**
         * @brief Command list to register this command to. It can the
         * global_cmd_list (default), a custom command list.
         * @note Required.
         */
        ln::StaticForwardList<Cmd> &cmd_list = Cmd::global_cmd_list;

        /**
         * @brief Parent command. It is base command if nullptr (default).
         * @note Optional.
         */
        Cmd *parent_cmd = nullptr;

        /**
         * @brief Command name tokens separated by commas, e.g. "help,?".
         * @note Required.
         */
        const char *name = nullptr;

        /**
         * @brief Command usage string, e.g. "[all|<command_name> [...]]".
         * @note Optional. May be unspecified if the command has no arguments.
         */
        const char *usage = nullptr;

        /**
         * @brief Short description in just a few words or up to around 60 to
         * make it fit in one line in the help output. E.g. "show command
         * usage".
         * @note Optional.
         */
        const char *short_description = nullptr;

        /**
         * @brief Long description that may include usage examples and command
         * details. https://docopt.org style is recommended.
         * @note Optional.
         */
        const char *long_description = nullptr;

        /**
         * @brief Command function to execute when the command is called.
         * @note Optional, no command by default. Functionless commands can be
         * used for grouping subcommands.
         */
        Fn fn = nullptr;
    };

    Cmd(Cfg cfg);

    void print_args(CLI &cli) const;
    void print_short_help(CLI &cli, std::size_t max_depth = 1, std::size_t depth = 0) const;
    void print_long_help(CLI &cli, std::size_t max_depth = 1, std::size_t depth = 0) const;

    static ln::StaticForwardList<Cmd> base_cmd_list;
    static ln::StaticForwardList<Cmd> general_cmd_list;
    static ln::StaticForwardList<Cmd> global_cmd_list;

private:
    friend CLI;

    static const Cmd *find_cmd_by_name(ln::StaticForwardList<Cmd> cmd_list, std::string_view name);
    const Cmd *find_child_cmd_by_name(std::string_view name) const;
    std::size_t resolve_cmd_depth() const;

    ln::StaticForwardList<Cmd> children_cmd_list;
    Cfg cfg;
};

} // namespace ln::shell
