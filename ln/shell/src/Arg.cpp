
/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/Arg.hpp"

#include <charconv>
#include <cstddef>

namespace ln::shell {
template <typename T> std::optional<T> from_chars_auto_base(std::string_view str) {
    T value = 0;
    enum Base : size_t {
        dec = 10,
        hex = 16
    };
    size_t base = Base::dec;
    if (str.length() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        base = Base::hex;
        str.remove_prefix(2);
    }
    const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.length(), value, base);
    if (ec != std::errc() || ptr != str.data() + str.length()) {
        return std::nullopt;
    }
    return value;
}

std::optional<uint32_t> Arg::as_u32() const { return from_chars_auto_base<uint32_t>(this->value); }

std::optional<int32_t> Arg::as_i32() const { return from_chars_auto_base<int32_t>(this->value); }

std::optional<float> Arg::as_f32() const {
    float value = 0.0;
    const auto [ptr, ec] = std::from_chars(this->value.data(), this->value.data() + this->value.length(), value);
    if (ec != std::errc() || ptr != this->value.data() + this->value.length()) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> Arg::as_f64() const {
    double value = 0.0;
    const auto [ptr, ec] = std::from_chars(this->value.data(), this->value.data() + this->value.length(), value);
    if (ec != std::errc() || ptr != this->value.data() + this->value.length()) {
        return std::nullopt;
    }
    return value;
}

bool Arg::is_alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

bool Arg::is_flag(const std::string_view arg) {
    if (arg.length() < 2) {
        return false;
    }
    if (arg[0] != '-') {
        return false;
    }
    if (!is_alpha(arg[1])) {
        return false;
    }
    return true;
}

bool Arg::is_option(const std::string_view arg) {
    if (arg.length() < 2) {
        return false;
    }
    if (arg[0] != '-') {
        return false;
    }
    if (!is_alpha(arg[1])) {
        return false;
    }
    return true;
}

} // namespace ln::shell
