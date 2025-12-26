/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "ln/File.hpp"
#include "ln/shell/Arg.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace ln::shell {

class ArgParser {
public:
    struct Cfg {
        std::span<const Arg> positional_args;
        static constexpr size_t args_buf_size_default = 16;
    };

    ArgParser(const Cfg &cfg, const std::span<const std::string_view> &args) : cfg{cfg}, args{args} {}

    [[nodiscard]] Arg get_positional(std::size_t index) const {
        if (index >= this->cfg.positional_args.size()) {
            return Arg{.role = Arg::Role::non_existent};
        }
        Arg arg = this->cfg.positional_args[index];
        if (index < this->args.size()) {
            arg.value = this->args[index];
        }
        return arg;
    }

    static std::optional<std::span<std::string_view>> tokenize(std::string_view sv,
                                                               std::span<std::string_view> args_buf);

    bool validate_arg_composition(File &ostream, std::span<const std::string_view> args) const;

    // TODO: private:
    const Cfg &cfg;
    const std::span<const std::string_view> &args;
};

} // namespace ln::shell
