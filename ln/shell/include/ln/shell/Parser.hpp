/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "ln/shell/Arg.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace ln::shell {

// TODO: consider rename to ArgParser.
class Parser {
public:
    static std::optional<std::span<std::string_view>> tokenize(const std::string_view sv,
                                                               std::span<std::string_view> args_buf);

    const std::span<const Arg> positional;
};

} // namespace ln::shell
