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
#include <optional>
#include <cstdint>
#include <cctype>

namespace ln::shell {

class Arg {
public:
    /**
     * @brief Syntactic role of the argument.
     */
    enum class Role : uint8_t {
        non_existent,
        positional,
        flag,
        option
    };

    /**
     * @brief Semantic type of the argument.
     */
    enum class Type : uint8_t {
        num,
        str,
    };

    Role role = Role::non_existent;
    std::string_view name = {};
    Type type = Type::str;
    std::string_view description = {};
    std::string_view default_value = {};
    std::string_view value = default_value = {};

    [[nodiscard]] std::optional<uint32_t> as_u32() const;
    [[nodiscard]] std::optional<int32_t> as_i32() const;
    [[nodiscard]] std::optional<float> as_f32() const;
    [[nodiscard]] std::optional<double> as_f64() const;

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

    // TODO private:
    static bool is_alpha(char c);
    static bool is_flag(std::string_view arg);
    static bool is_option(std::string_view arg);
};

} // namespace ln::shell
