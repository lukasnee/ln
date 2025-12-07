/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/Args.hpp"

#include <string_view>
#include <algorithm>
#include <cstdio>
#include <optional>
#include <ranges>

namespace ln::shell {

std::optional<std::span<std::string_view>> Args::tokenize(const std::string_view sv,
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

std::optional<std::span<std::string_view>> Args::copy(const std::span<char> dst_str_buf,
                                                      std::span<std::string_view> dst_args_buf,
                                                      std::span<const std::string_view> src_args) {
    if (src_args.size() > dst_args_buf.size()) {
        return std::nullopt;
    }
    size_t dst_str_buf_used = 0;
    for (auto [arg_idx, arg] : std::views::enumerate(src_args)) {
        const auto dst_str_buf_left = dst_str_buf.size() - dst_str_buf_used;
        if (dst_str_buf_left <= arg.size()) {
            return std::nullopt;
        }
        const auto dst_str_buf_head = dst_str_buf.data() + dst_str_buf_used;
        std::copy_n(arg.data(), arg.size(), dst_str_buf_head);
        dst_args_buf[arg_idx] = std::string_view(dst_str_buf_head, arg.size());
        dst_str_buf_used += arg.size();
    }
    return dst_args_buf.first(src_args.size());
}

} // namespace ln::shell
