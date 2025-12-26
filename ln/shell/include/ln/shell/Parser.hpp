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
        static constexpr size_t args_buf_size_default = 16;
    };

    ArgParser(const std::span<const Arg> &arg_cfg, const std::span<const std::string_view> &input_args)
        : arg_cfg{arg_cfg}, input_args{input_args} {}

    [[nodiscard]] Arg get_positional(std::size_t index) const {
        if (index >= this->arg_cfg.size()) {
            return Arg{.role = Arg::Role::non_existent};
        }
        Arg arg = this->arg_cfg[index];
        if (index < this->input_args.size()) {
            arg.value = this->input_args[index];
        }
        return arg;
    }

    static std::optional<std::span<std::string_view>> tokenize(std::string_view sv,
                                                               std::span<std::string_view> args_buf);

    bool validate_arg_composition(File &ostream, std::span<const std::string_view> input_args) const;

    // TODO: private:
    const std::span<const Arg> &arg_cfg;
    const std::span<const std::string_view> &input_args;
};

} // namespace ln::shell
