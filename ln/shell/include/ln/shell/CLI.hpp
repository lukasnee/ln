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
#include "ln/shell/Input.hpp"
#include "ln/shell/Cmd.hpp"

#include "FreeRTOS/Addons/Clock.hpp"

#include <array>
#include <span>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <tuple>

// TODO: extract color code for logger and CLI
// TODO: rework the OK FAIL tags, color codes, etc. Make the interface cleaner and flexible.
#define ANSI_COLOR_BLACK "\e[30m"
#define ANSI_COLOR_RED "\e[31m"
#define ANSI_COLOR_GREEN "\e[32m"
#define ANSI_COLOR_YELLOW "\e[33m"
#define ANSI_COLOR_BLUE "\e[34m"
#define ANSI_COLOR_MAGENTA "\e[35m"
#define ANSI_COLOR_CYAN "\e[36m"
#define ANSI_COLOR_WHITE "\e[37m"
#define ANSI_COLOR_DEFAULT "\e[39m"
#define ANSI_COLOR_RESET "\e[0m"

namespace ln::shell {

class CLI {
public:
    struct Config {
        File ostream = File(stdout);
        static constexpr std::size_t printf_buffer_size = 256;
        static constexpr bool regular_response_is_enabled = true;
        bool colored_output = true;
        bool print_result_tags = false;
        static inline std::array<ln::StaticForwardList<Cmd> *, 3> default_cmd_lists = {
            &Cmd::base_cmd_list, &Cmd::general_cmd_list, &Cmd::global_cmd_list};
        std::span<ln::StaticForwardList<Cmd> *> cmd_lists = default_cmd_lists;
    } config;

    explicit CLI(std::span<char> input_line_buf) : input{input_line_buf} {}

    // NOTE: escape sequences are time sensitive !
    // TODO: move this to a dedicated uart receiver task and join by char queue
    // escape sequence finished (not time sensitive)
    bool put_char(const char &c);

    void print(const char &c, std::size_t times_to_repeat = 1);
    int print(const char *str);
    int print(std::string_view sv);
    int printf(const char *fmt, ...);

    /** @return {cmd, args} */
    std::tuple<const Cmd *, std::span<const std::string_view>> find_cmd(std::span<const std::string_view> args);

    bool execute_line(std::string_view line);

private:
    Err execute(const Cmd &cmd, std::span<const std::string_view> args,
                const char *output_color_escape_sequence = "\e[32m"); // default in green

    bool handle_escape(const char &c);
    bool handle_ansi_escape(const char &c);
    bool handle_ansi_delimited_escape(const char &c);
    bool handle_ansi_delimited_del_escape(const char &c);
    bool delete_char();
    bool on_home_key();
    bool on_arrow_up_key();
    bool on_arrow_down_key();
    bool on_arrow_right_key();
    bool on_arrow_left_key();

    bool backspace_char();
    bool insert(const char &c);
    void print_prompt();

    enum class EscapeState : std::int8_t {
        failed = -1,
        none = 0,
        escaped,
        delimited,
        intermediate,
        finished,
    } escape_state;

    FreeRTOS::Addons::Clock::time_point escape_start_time;

    Input input;
    Err last_err = Err::ok;
};
} // namespace ln::shell
