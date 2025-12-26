/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/Parser.hpp"
#include "ln/shell/Arg.hpp"

#include <string_view>
#include <cstdio>
#include <optional>
#include <ranges>

namespace ln::shell {

std::optional<std::span<std::string_view>> ArgParser::tokenize(const std::string_view sv,
                                                               std::span<std::string_view> args_buf) {
    size_t arg_count = 0;
    const char *arg_begin = nullptr;
    char quote_char = '\0';
    for (const char *head = sv.data(); static_cast<size_t>(head - sv.data()) < sv.length() && *head != '\0'; head++) {
        if (quote_char != '\0') {
            if (*head == quote_char) {
                quote_char = '\0';
                if (arg_begin) {
                    if (arg_count >= args_buf.size()) {
                        return std::nullopt; // not enough space in args_buf
                    }
                    args_buf[arg_count++] = std::string_view(arg_begin, head - arg_begin);
                    arg_begin = nullptr;
                }
            }
        }
        else {
            if (*head == '\'' || *head == '"') {
                if (arg_begin) {
                    if (arg_count >= args_buf.size()) {
                        return std::nullopt; // not enough space in args_buf
                    }
                    args_buf[arg_count++] = std::string_view(arg_begin, head - arg_begin);
                }
                quote_char = *head;
                arg_begin = head + 1;
            }
            else if (*head == ' ') {
                if (!arg_begin) {
                    continue;
                }
                if (arg_count >= args_buf.size()) {
                    return std::nullopt; // not enough space in args_buf
                }
                args_buf[arg_count++] = std::string_view(arg_begin, head - arg_begin);
                arg_begin = nullptr;
            }
            else if (!arg_begin) {
                arg_begin = head;
            }
        }
    }
    if (arg_begin) {
        if (arg_count >= args_buf.size()) {
            return std::nullopt;
        }
        if (quote_char != '\0') {
            return std::nullopt; // unmatched quote
        }
        args_buf[arg_count++] = std::string_view(arg_begin, sv.data() + sv.length() - arg_begin);
    }
    return args_buf.first(arg_count);
}

bool ArgParser::validate_arg_composition(File &ostream, std::span<const std::string_view> args) const {
    size_t positional_arg_count = 0;
    for (const auto &arg_cfg : this->arg_cfg) {
        if (arg_cfg.role == Arg::Role::positional) {
            positional_arg_count++;
        }
    }
    if (args.size() < positional_arg_count) {
        std::fprintf(ostream.c_file(), "Error: not enough arguments (expected %zu, got %zu)\n", positional_arg_count,
                     args.size());
        return false;
    }
    for (const auto [i, arg_cfg] : std::views::enumerate(this->arg_cfg)) {
        auto arg = args[i];
        if (arg.empty()) {
            std::fprintf(ostream.c_file(), "Error: expected non-empty positional argument %zu\n", i);
            return false;
        }
        switch (arg_cfg.type) {
        case Arg::Type::num: {
            // TODO
        }
        case Arg::Type::str:
            // no additional validation for strings
            break;
        }
    }
    return true;
}

} // namespace ln::shell
