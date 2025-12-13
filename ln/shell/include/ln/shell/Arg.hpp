/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <string_view>

namespace ln::shell {

class Arg {
public:
    std::string_view name;
    enum class Type {
        num,
        str,
    } type = Type::str;
    std::string_view description;

    static std::string_view to_string(Arg::Type type) {
        switch (type) {
        case Arg::Type::num:
            return "num";
        case Arg::Type::str:
            return "str";
        default:
            return "unknown";
        }
    }
};

} // namespace ln::shell
