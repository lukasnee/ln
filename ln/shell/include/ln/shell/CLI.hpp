// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/File.hpp"
#include "ln/Interpreter.hpp"
#include "ln/shell/History.hpp"
#include "ln/shell/Buffer.hpp"
#include "ln/shell/Cmd.hpp"

#include "ln/log.h"

#include <array>
#include <span>
#include <cstdarg>
#include <cstring>
#include <string_view>
#include <tuple>

// TODO: extract color code for logger and CLI
// TODO: rework the OK FAIL tags, color codes, etc. Make the interface cleaner
// and flexible.
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
    enum class Mode {
        command,
        interpreter,
    };

    struct Config {
        File istream = File(stdin);
        File ostream = File(stdout);
        static constexpr std::size_t fmt_output_buf = 256;
        static constexpr bool regular_response_is_enabled = true;
        bool colored_output = true;
        bool print_result_tags = false;
        Interpreter *interpreter = nullptr;
        static inline std::array<Cmd::List *, 3> default_cmd_lists = {
            &Cmd::get_base_cmd_list(), &Cmd::get_general_cmd_list(),
            &Cmd::get_global_cmd_list()};
        std::span<Cmd::List *> cmd_lists = default_cmd_lists;
        std::string_view command_mode_prompt_str = "$ ";
        std::string_view interpreter_prompt_str = "> ";
        std::string_view interpreter_multiline_prompt_str = ">> ";
    } config;

    explicit CLI(std::span<char> input_buf, std::span<char> history_buf = {});
    void reset();
    void routine();

    void print(const char &c, std::size_t times_to_repeat = 1);
    int print(const char *str);
    int print(std::string_view sv);

    template <typename... Args>
    int print(fmt::string_view fmt_str, Args &&...args) {
        std::array<char, Config::fmt_output_buf> tx_buf;
        auto res = fmt::vformat_to_n(tx_buf.data(), tx_buf.size(), fmt_str,
                                     fmt::make_format_args(args...));
        if (res.size <= 0) {
            return static_cast<int>(res.size);
        }
        return this->print(
            std::string_view(tx_buf.data(), static_cast<size_t>(res.size)));
    }

    using Args = std::span<const std::string_view>;

    std::tuple<const Cmd *, Args> find_cmd(Args args);

    Err execute(std::string_view input);

    void clear_screen();

private:
    Err execute(const Cmd &cmd, Args args,
                const char *output_color_escape_sequence = ANSI_COLOR_GREEN);

    char getc_or_handle_escape_sequences();

    void add_entry_to_history(std::string_view entry);
    bool recall_prev_entry_from_history();
    bool recall_next_entry_from_history();

    void print_prompt(bool is_multiline = false);
    void print_prompt_multiline();
    size_t get_prompt_length() const;

    // TODO: try to extract class Editor for visual input editing. It should
    // take Input as a parameter as well as the starting cursor position. Maybe
    // define a scrollable region for the input buffer.

    bool step_cursor_left();
    bool step_cursor_left_word();
    bool step_cursor_right();
    bool step_cursor_right_word();
    bool move_cursor_begin();
    bool move_cursor_end();

    void clear_input();
    bool delete_char();
    bool backspace_char();
    /** @return true if actually inserted */
    bool insert(char c);

    LN_LOG_MODULE_CLASS_MEMBER(CLI, ln::logger::Level::notset);

    Buffer input;
    friend class History;
    History history;
    bool previously_called_from_history = false;
    Err last_err = Err::ok;
    Mode mode = Mode::command;
    bool pasting_mode = false;
};
} // namespace ln::shell
