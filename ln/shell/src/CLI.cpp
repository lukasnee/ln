/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/CLI.hpp"
#include "ln/shell/Parser.hpp"
// TODO: make arrow up repeat buffer
// TODO: some kind of esacpe signal mechanism to inform running cmd to exit.

#include <cstdio>
#include <cstring>
#include <type_traits>

namespace ln::shell {

void CLI::print(const char &c, std::size_t times_to_repeat) {
    while (times_to_repeat--) {
        if (c == '\n') {
            std::fputc('\r', this->config.ostream.c_file());
        }
        std::fputc(c, this->config.ostream.c_file());
    }
    std::fflush(this->config.ostream.c_file());
}

int CLI::print(std::string_view sv) {
    const auto rc = static_cast<int>(std::fwrite(sv.data(), 1, sv.size(), this->config.ostream.c_file()));
    std::fflush(this->config.ostream.c_file());
    return rc;
}

int CLI::print(const char *str) {
    const auto rc = std::fputs(str, this->config.ostream.c_file());
    std::fflush(this->config.ostream.c_file());
    return rc;
}

int CLI::printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::array<char, Config::printf_buffer_size> tx_buf;
    vsnprintf(tx_buf.data(), tx_buf.size(), fmt, args);
    int chars_printed = this->print(tx_buf.data());
    va_end(args);
    return chars_printed;
}

std::tuple<const Cmd *, std::span<const std::string_view>> CLI::find_cmd(std::span<const std::string_view> args) {
    if (args.empty()) {
        return {};
    }
    if (args[0].empty()) {
        return {};
    }
    for (const auto &cmd_list_ptr : this->config.cmd_lists) {
        if (!cmd_list_ptr) {
            continue;
        }
        std::size_t arg_offset = 0;
        auto cmd = Cmd::find_cmd_by_name(*cmd_list_ptr, args[arg_offset]);
        if (!cmd) {
            continue;
        }
        while (args.size() - arg_offset - 1) {
            const Cmd *child_cmd = cmd->find_child_cmd_by_name(args[arg_offset + 1]);
            if (!child_cmd) {
                break;
            }
            arg_offset++;
            cmd = child_cmd;
        }
        return {cmd, args.subspan(arg_offset + 1)};
    }
    return {};
}

Err CLI::execute(const Cmd &cmd, const std::span<const std::string_view> args,
                 const char *output_color_escape_sequence) {
    if (!cmd.cfg.fn) {
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_RED);
        }
        this->print("command has no function\n");
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_RESET);
        }
        return Err::unexpected;
    }
    ArgParser argp{cmd.cfg.args, args};
    if (!argp.validate_arg_composition(this->config.ostream, args)) {
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_YELLOW);
        }
        this->print("bad arguments\n");
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_RESET);
        }
        return Err::badArg;
    }
    this->print(output_color_escape_sequence); // response in green
    const auto err = cmd.cfg.fn(Cmd::Ctx{*this, argp, args});
    if (!Config::regular_response_is_enabled) {
        return err;
    }
    if (err == Err::badArg) {
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_YELLOW);
        }
        this->print("bad arguments\n");
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_RESET);
        }
        return err;
    }
    if (err == Err::ok) {
        if (this->config.print_result_tags) {
            if (this->config.colored_output) {
                this->print(ANSI_COLOR_GREEN);
            }
            this->print("\nOK");
            if (this->config.colored_output) {
                this->print(ANSI_COLOR_RESET);
            }
        }
    }
    else if (static_cast<std::int8_t>(err) < 0) {
        if (this->config.print_result_tags) {
            if (this->config.colored_output) {
                this->print(ANSI_COLOR_RED);
            }
            this->print("\nFAIL");
            if (err != Err::fail) {
                this->printf(" (%d)", static_cast<std::underlying_type_t<decltype(err)>>(err));
            }
            if (this->config.colored_output) {
                this->print(ANSI_COLOR_RESET);
            }
        }
    }
    return err;
}

/** @return true if sequence finished */
bool CLI::put_char(const char &c) {
    if (this->handle_escape(c)) {
        return false;
    }
    if (c == '\x7F') {
        this->backspace_char();
        return true;
    }
    if (' ' <= c && c <= '~') {
        if (this->is_prompted) {
            this->input.clear();
            this->is_prompted = false;
        }
        this->insert_char(c);
        return true;
    }
    if (c == '\r') {
        this->print("\r\n");
        const auto res = this->execute_line(this->input.get());
        this->input.clear();
        this->is_prompted = true;
        this->print_prompt();
        return res;
    }
    return false;
}

bool CLI::execute_line(std::string_view line) {
    std::array<std::string_view, ArgParser::Cfg::args_buf_size_default> args_buf;
    auto opt_args = ArgParser::tokenize(line, args_buf);
    if (!opt_args) {
        this->last_err = Err::badArg;
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_RED);
        }
        this->print("error parsing arguments\n");
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_RESET);
        }
        return false;
    }
    const auto args = *opt_args;
    if (args.empty()) {
        this->last_err = Err::ok;
        return true;
    }
    const auto [cmd, cmd_args] = this->find_cmd(args);
    if (!cmd) {
        this->last_err = Err::unknownCmd;
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_RED);
        }
        this->print("command not found\n");
        if (this->config.colored_output) {
            this->print(ANSI_COLOR_RESET);
        }
        return false;
    }
    this->last_err = this->execute(*cmd, cmd_args);
    return true;
}

/** @result false - nothing to handle */
bool CLI::handle_escape(const char &c) {
    if (c == '\e') {
        this->escape_start_time = FreeRTOS::Addons::Clock::now();
        this->escape_state = EscapeState::escaped;
        return true;
    }
    if (this->escape_state != EscapeState::escaped && this->escape_state != EscapeState::delimited &&
        this->escape_state != EscapeState::intermediate && this->escape_state != EscapeState::finished) {
        this->escape_state = EscapeState::none; // unexpected state
        return false;
    }
    if (FreeRTOS::Addons::Clock::now() - this->escape_start_time > std::chrono::milliseconds(2)) {
        /* timed out */
        this->escape_state = EscapeState::none;
        return false;
    }
    const char ascii_char_del = 0x7F;
    if (c == ascii_char_del) {
        delete_char();
        this->escape_state = EscapeState::none;
        return true;
    }
    return this->handle_ansi_escape(c);
}

/** @result false - nothing to handle */
bool CLI::handle_ansi_escape(const char &c) {
    if (c == '[') /* open delimiter */
    {
        this->escape_state = EscapeState::delimited;
        return true;
    }
    if (this->escape_state != EscapeState::delimited && this->escape_state != EscapeState::intermediate &&
        this->escape_state != EscapeState::finished) {
        this->escape_state = EscapeState::failed;
        return false;
    }
    return this->handle_ansi_delimited_escape(c);
}

bool CLI::handle_ansi_delimited_escape(const char &c) {
    if (this->handle_ansi_delimited_del_escape(c)) {
        return true;
    }
    if (c == 'H') {
        this->on_home_key();
        this->escape_state = EscapeState::finished;
        return true;
    }
    if (c == 'A') {
        this->on_arrow_up_key();
        this->escape_state = EscapeState::finished;
        return true;
    }
    if (c == 'B') {
        this->on_arrow_down_key();
        this->escape_state = EscapeState::finished;
        return true;
    }
    if (c == 'C') {
        this->on_arrow_right_key();
        this->escape_state = EscapeState::finished;
        return true;
    }
    if (c == 'D') {
        this->on_arrow_left_key();
        this->escape_state = EscapeState::finished;
        return true;
    }
    this->escape_state = EscapeState::failed;
    return false;
}

bool CLI::handle_ansi_delimited_del_escape(const char &c) {
    if ((this->escape_state == EscapeState::delimited || this->escape_state == EscapeState::intermediate ||
         this->escape_state == EscapeState::finished) &&
        c == '3') {
        this->escape_state = EscapeState::intermediate;
        return true;
    }
    if ((this->escape_state == EscapeState::intermediate || this->escape_state == EscapeState::finished) && c == '~') {
        this->delete_char();
        this->escape_state = EscapeState::finished;
        return true;
    }
    this->escape_state = EscapeState::failed;
    return false;
}

bool CLI::delete_char() {
    if (!this->input.delete_char()) {
        return false;
    }
    this->print(this->input.get().substr(this->input.get_cursor_pos()));
    this->print(" \b");
    this->print('\b', this->input.get().size() - this->input.get_cursor_pos());
    return true;
}

bool CLI::on_home_key() {
    while (this->on_arrow_left_key()) {
    }
    return true;
}

bool CLI::on_arrow_up_key() {
    // TODO: implement history buffer; consider using args.untokenize(), if
    // it turns out unuseful - remove it.
    return false;
}

bool CLI::on_arrow_down_key() {
    this->input.clear();
    this->is_prompted = true;
    return true;
}

bool CLI::on_arrow_left_key() {
    if (!this->input.step_left()) {
        return false;
    }
    this->print('\b');
    return true;
}

bool CLI::on_arrow_right_key() {
    if (!this->input.step_right()) {
        return false;
    }
    this->print(this->input.get().substr(this->input.get_cursor_pos() - 1, 1));
    return true;
}

void CLI::print_prompt(void) {
    if (this->config.colored_output) {
        this->print(this->last_err == Err::ok ? ANSI_COLOR_GREEN : ANSI_COLOR_RED);
    }
    this->print("> ");
    if (this->config.colored_output) {
        this->print(ANSI_COLOR_YELLOW);
    }
}

/** @return true if actually backspaced */
bool CLI::backspace_char() {
    if (!this->input.backspace_char()) {
        return false;
    }
    this->print("\b");
    this->print(this->input.get().substr(this->input.get_cursor_pos()));
    this->print(" \b");
    this->print('\b', this->input.get().size() - this->input.get_cursor_pos());
    return true;
}

/** @return true if actually inserted */
bool CLI::insert_char(const char &c) {
    if (!this->input.insert_char(c)) {
        return false;
    }
    this->print(this->input.get().substr(this->input.get_cursor_pos() - 1));
    this->print('\b', this->input.get().size() - this->input.get_cursor_pos());
    return true;
}

} // namespace ln::shell
