/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace ln::shell {

class Args {
public:
    
    static std::optional<std::span<std::string_view>> tokenize(const std::string_view sv,
                                                               std::span<std::string_view> args_buf);
    static std::optional<std::span<std::string_view>> copy(const std::span<char> dst_str_buf,
                                                           std::span<std::string_view> dst_args_buf,
                                                           std::span<const std::string_view> src_args);

    /**
     * @brief copy out arguments (contents) by given format into buffer.
     *
     * @param delimiter Delimiter put printed after every argument, e.g.: " ", "\n"...
     * @param buf Buffer to print to.
     * @param size Buffer to print to size (max).
     * @param nullSeparated Puts null at the end of every delimiter between arguments. passing empty delimiter "" to
     * copy arguments to some other buffer
     * @return true Printed successfully.
     * @return false Could not format the delimter (delimiter too long) or could not finish printing (no space left in
     * buffer).
     */
    static bool print_to(char *buf, std::size_t size, const char *delimiter = " ", bool nullSeparated = false);
};

} // namespace ln::shell
