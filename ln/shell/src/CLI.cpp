// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/shell/CLI.hpp"
#include "ln/shell/Parser.hpp"
// TODO: some kind of escape signal mechanism to inform running cmd to exit.

#include <cstdio>
#include <cstring>
#include <ranges>
#include <type_traits>

namespace ln::shell {

CLI::CLI(std::span<char> input_buf, std::span<char> history_buf)
    : input{input_buf}, history{history_buf} {
    this->reset();
}

void CLI::print(const char &c, size_t times_to_repeat) {
    while (times_to_repeat--) {
        if (c == '\n') {
            std::fputc('\r', this->config.ostream.c_file());
        }
        std::fputc(c, this->config.ostream.c_file());
    }
    std::fflush(this->config.ostream.c_file());
}

int CLI::print(std::string_view sv) {
    const auto rc = static_cast<int>(
        std::fwrite(sv.data(), 1, sv.size(), this->config.ostream.c_file()));
    std::fflush(this->config.ostream.c_file());
    return rc;
}

int CLI::print(const char *str) {
    const auto rc = std::fputs(str, this->config.ostream.c_file());
    std::fflush(this->config.ostream.c_file());
    return rc;
}

std::tuple<const Cmd *, std::span<const std::string_view>> CLI::find_cmd(
    std::span<const std::string_view> args) {
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
        auto cmd_ptr = Cmd::find_cmd_by_name(*cmd_list_ptr, args[arg_offset]);
        if (!cmd_ptr) {
            continue;
        }
        while (args.size() - arg_offset - 1) {
            const Cmd *child_cmd =
                cmd_ptr->find_child_cmd_by_name(args[arg_offset + 1]);
            if (!child_cmd) {
                break;
            }
            arg_offset++;
            cmd_ptr = child_cmd;
        }
        return {cmd_ptr, args.subspan(arg_offset + 1)};
    }
    return {};
}

// TODO: make first arg the name of the function
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
                this->print(
                    " ({})",
                    static_cast<std::underlying_type_t<decltype(err)>>(err));
            }
            if (this->config.colored_output) {
                this->print(ANSI_COLOR_RESET);
            }
        }
    }
    return err;
}

void CLI::reset() {
    // Tell terminal to enable bracketed paste mode
    this->print("\033[?2004h");
}

void CLI::routine() {
    const char c = this->getc_or_handle_escape_sequences();
    if (c == '\x7F') {
        this->backspace_char();
        return;
    }
    if (' ' <= c && c <= '~') {
        this->insert(c);
        return;
    }
    if (this->pasting_mode && c == '\r') {
        this->insert('\n');
        return;
    }
    if (c == '\r') {
        this->print("\r\n");
        this->add_entry_to_history(this->input.get());
        if (this->mode == Mode::command) {
            this->last_err = this->execute(this->input.get());
            if (this->last_err == Err::unknownCmd) {
                if (this->config.colored_output) {
                    this->print(ANSI_COLOR_RED);
                }
                this->print("command not found\n");
                if (this->config.colored_output) {
                    this->print(ANSI_COLOR_RESET);
                }
            }
        }
        else if (this->mode == Mode::interpreter) {
            if (!this->config.interpreter) {
                this->print("No interpreter configured.\n");
            }
            const auto err =
                this->config.interpreter->interpret(this->input.get());
            if (err == Interpreter::Err::incomplete) {
                this->last_err = Err::incomplete;
                this->input.insert('\n');
                this->print_prompt_multiline();
                return;
            }
            if (err == Interpreter::Err::ok) {
                this->last_err = Err::ok;
            }
            else if (err == Interpreter::Err::compileError ||
                     err == Interpreter::Err::runtimeError) {
                this->last_err = Err::fail;
            }
            else {
                this->last_err = Err::unexpected;
            }
        }
        this->input.clear();
        // TODO: it can be that visual cursor is not at end of line here.
        // We should move it to the end at this point.
        this->print_prompt();
    }
}

Err CLI::execute(std::string_view input) {
    if (input.empty()) {
        return Err::ok;
    }
    for (auto cmd_range : input | std::views::split(std::string_view{"\\n"})) {
        const std::string_view cmd{cmd_range.begin(), cmd_range.end()};
        std::array<std::string_view, ArgParser::Cfg::args_buf_size_default>
            args_buf;
        auto opt_args = ArgParser::tokenize(cmd, args_buf);
        if (!opt_args) {
            if (this->config.colored_output) {
                this->print(ANSI_COLOR_RED);
            }
            this->print("error parsing arguments\n");
            if (this->config.colored_output) {
                this->print(ANSI_COLOR_RESET);
            }
            return Err::badArg;
        }
        const auto args = *opt_args;
        if (args.empty()) {
            return Err::ok;
        }
        const auto [cmd_ptr, cmd_args] = this->find_cmd(args);
        if (!cmd_ptr) {
            return Err::unknownCmd;
        }
        auto err = this->execute(*cmd_ptr, cmd_args);
        if (err != Err::ok) {
            return err;
        }
    }
    return Err::ok;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
char CLI::getc_or_handle_escape_sequences() {
    const size_t warn_buf_capacity =
        10; // enough to hold any escape sequence we care about
    std::array<char, warn_buf_capacity> warn_buf;
    std::size_t buf_size = 0;
    auto getc = [&]() {
        const auto c =
            static_cast<char>(std::fgetc(this->config.istream.c_file()));
        if (buf_size < warn_buf.size()) {
            warn_buf[buf_size++] = c == '\e' ? 'e' : c;
        }
        return c;
    };
    auto handle_unknown = [&]() {
        LOG_WARNING("Unknown escape sequence: {}",
                    std::string_view(warn_buf.data(), buf_size));
    };
    while (true) {
        char c = getc();

        enum AsciiControlChar : char {
            ctrl_l = 0x0C,
            ctrl_z = 0x1A
        };
        if (c == AsciiControlChar::ctrl_l) {
            this->clear_screen();
            continue;
        }
        if (c == AsciiControlChar::ctrl_z) {
            if (this->mode == Mode::command) {
                if (!this->config.interpreter) {
                    this->print("No interpreter configured.\n");
                    continue;
                }
                this->mode = Mode::interpreter;
                this->print("\nSwitched to interpreter mode. "
                            "Press Ctrl+Z to exit.\n");
            }
            else if (this->mode == Mode::interpreter) {
                this->mode = Mode::command;
                this->print("\nSwitched to command mode. "
                            "Press Ctrl+Z to enter interpreter mode.\n");
            }
            this->print_prompt();
            continue;
        }
        if (c != '\e') {
            return c;
        }
        c = getc(); /* if this takes too long, we should trigger "escape key"
                       action TODO: implement timeout. For now, we can trigger
                       an action after two consecutive escape characters. */
        if (c == '\e') { // Escape key (twice)
            this->clear_input();
            continue;
        }
        if (c == '\r') { // Alt + Enter
            this->insert('\n');
            continue;
        }
        if (c == '[') {
            c = getc();
            if (c == 'A') { // Up arrow
                this->recall_prev_entry_from_history();
                continue;
            }
            if (c == 'B') { // Down arrow
                this->recall_next_entry_from_history();
                continue;
            }
            if (c == 'C') { // Right arrow
                this->step_cursor_right();
                continue;
            }
            if (c == 'D') { // Left arrow
                this->step_cursor_left();
                continue;
            }
            if (c == 'H') { // Home key
                this->move_cursor_begin();
                continue;
            }
            if (c == 'F') { // End key
                this->move_cursor_end();
                continue;
            }
            if (c == '1') {
                c = getc();
                if (c != ';') {
                    handle_unknown();
                    continue;
                }
                c = getc();
                if (c != '5') {
                    handle_unknown();
                    continue;
                }
                c = getc();
                if (c == 'C') { // Ctrl + Right arrow
                    this->step_cursor_right_word();
                    continue;
                }
                if (c == 'D') { // Ctrl + Left arrow
                    this->step_cursor_left_word();
                    continue;
                }
            }
            if (c == '2') {
                c = getc();
                if (c == '0') {
                    c = getc();
                    if (c == '0') {
                        c = getc();
                        if (c == '~') { // Bracketed paste start
                            this->pasting_mode = true;
                            continue;
                        }
                    }
                    else if (c == '1') {
                        c = getc();
                        if (c == '~') { // Bracketed paste end
                            this->pasting_mode = false;
                            continue;
                        }
                    }
                }
            }
            if (c == '3') {
                c = getc();
                if (c == '~') { // Delete key
                    this->delete_char();
                    continue;
                }
            }
        }
        handle_unknown();
    }
}

void CLI::add_entry_to_history(std::string_view entry) {
    if (std::ranges::equal(entry, this->history.get())) {
        this->previously_called_from_history = true;
    }
    else {
        this->history.add(entry);
    }
}

bool CLI::recall_prev_entry_from_history() {
    std::ranges::subrange<ln::RingBufferView<char>::iterator> entry = {};
    if (this->previously_called_from_history) {
        this->previously_called_from_history = false;
        entry = this->history.get();
    }
    else {
        entry = this->history.previous();
    }
    if (entry.empty()) {
        return false;
    }
    this->clear_input();
    for (const char &c : entry) {
        this->insert(c);
    }
    return true;
}

bool CLI::recall_next_entry_from_history() {
    auto entry = this->history.next();
    if (entry.empty()) {
        return false;
    }
    this->clear_input();
    for (const char &c : entry) {
        this->insert(c);
    }
    return true;
}

bool CLI::step_cursor_left() {
    if (!this->input.step_left()) {
        return false;
    }
    if (this->input.get_char_at_cursor() != '\n') {
        this->print('\b');
        return true;
    }
    this->print("\e[1A");
    auto distance = this->input.get_distance_to_prev('\n');
    if (distance == this->input.get_cursor_pos()) {
        distance += this->get_prompt_length();
    }
    if (distance > 0) {
        this->print("\e[{}C", distance);
    }
    return true;
}

bool CLI::step_cursor_left_word() {
    char c = this->input.get_char_at_cursor(-1);
    bool isspace_to_skip = std::isspace(c);
    bool isalnum_to_skip = std::isalnum(c);
    while (this->step_cursor_left()) {
        c = this->input.get_char_at_cursor(-1);
        if (isspace_to_skip) {
            if (std::isspace(c)) {
                continue;
            }
            break;
        }
        if (isalnum_to_skip) {
            if (std::isalnum(c)) {
                continue;
            }
            break;
        }
        if (std::isspace(c)) {
            isspace_to_skip = true;
            continue;
        }
        if (std::isalnum(c)) {
            isalnum_to_skip = true;
            continue;
        }
        break;
    }
    return true;
}

bool CLI::step_cursor_right() {
    if (!this->input.step_right()) {
        return false;
    }
    this->print(this->input.get().substr(this->input.get_cursor_pos() - 1, 1));
    return true;
}

bool CLI::step_cursor_right_word() {
    const char c = this->input.get_char_at_cursor();
    bool isspace_to_skip = std::isspace(c);
    bool isalnum_to_skip = std::isalnum(c);
    while (this->step_cursor_right()) {
        const char c = this->input.get_char_at_cursor();
        if (isspace_to_skip) {
            if (std::isspace(c)) {
                continue;
            }
            break;
        }
        if (isalnum_to_skip) {
            if (std::isalnum(c)) {
                continue;
            }
            break;
        }
        if (std::isspace(c)) {
            isspace_to_skip = true;
            continue;
        }
        if (std::isalnum(c)) {
            isalnum_to_skip = true;
            continue;
        }
        break;
    }
    return true;
}

bool CLI::move_cursor_begin() {
    // TODO: optimize
    while (this->step_cursor_left()) {
    }
    return true;
}

bool CLI::move_cursor_end() {
    // TODO: optimize
    while (this->step_cursor_right()) {
    }
    return true;
}

void CLI::print_prompt(bool is_multiline) {
    if (this->config.colored_output) {
        if (this->last_err == Err::ok || this->last_err == Err::incomplete) {
            this->print(ANSI_COLOR_GREEN);
        }
        else {
            this->print(ANSI_COLOR_RED);
        }
    }
    if (this->mode == Mode::command) {
        this->print(this->config.command_mode_prompt_str);
    }
    else if (this->mode == Mode::interpreter) {
        this->print(is_multiline ? this->config.interpreter_multiline_prompt_str
                                 : this->config.interpreter_prompt_str);
    }
    if (this->config.colored_output) {
        this->print(ANSI_COLOR_YELLOW);
    }
}

void CLI::print_prompt_multiline() { this->print_prompt(true); }

size_t CLI::get_prompt_length() const {
    switch (this->mode) {
    case Mode::command:
        return this->config.command_mode_prompt_str.size();
    case Mode::interpreter:
        return this->config.interpreter_prompt_str.size();
    default:
        return 0;
    }
}

void CLI::clear_screen() {
    this->print("\e[2J\e[H");
    this->print_prompt();
    this->print(this->input.get().substr(0, this->input.get_cursor_pos()));
    this->print("\e[s");
    this->print(this->input.get().substr(this->input.get_cursor_pos()));
    this->print("\e[u");
}

void CLI::clear_input() {
    const auto lines_back =
        std::count(this->input.get().begin(), this->input.cursor(), '\n');
    if (lines_back > 0) {
        this->print("\e[{}F\e[{}C", lines_back, this->get_prompt_length());
    }
    else {
        auto num_of_chars_on_left = this->input.get_distance_to_begin();
        if (num_of_chars_on_left > 0) {
            this->print("\e[{}D", num_of_chars_on_left);
        }
    }
    this->print("\e[J", this->get_prompt_length());
    this->input.clear();
}

bool CLI::delete_char() {
    const char removed_char = this->input.get_char_at_cursor();
    if (!this->input.delete_char()) {
        return false;
    }
    if (removed_char != '\n') {
        this->print("\e[P");
        return true;
    }
    this->print("\e[1B\e[M\e[1A");
    auto num_chars_on_left = this->input.get_distance_to_prev('\n');
    if (num_chars_on_left == this->input.get_cursor_pos()) {
        num_chars_on_left += this->get_prompt_length();
    }
    if (num_chars_on_left > 0) {
        this->print("\e[{}C", num_chars_on_left);
    }
    auto num_chars_on_right = this->input.get_distance_to_next('\n');
    if (num_chars_on_right > 0) {
        this->print(this->input.get().substr(this->input.get_cursor_pos(),
                                             num_chars_on_right));
        this->print("\e[{}D", num_chars_on_right);
    }
    return true;
}

/** @return true if actually backspaced */
bool CLI::backspace_char() {
    const char removed_char = this->input.get_char_at_cursor(-1);
    if (!this->input.backspace_char()) {
        return false;
    }
    if (removed_char != '\n') {
        this->print("\b\e[P");
        return true;
    }
    this->print("\e[M\e[1A");
    auto num_chars_on_left = this->input.get_distance_to_prev('\n');
    if (num_chars_on_left == this->input.get_cursor_pos()) {
        num_chars_on_left += this->get_prompt_length();
    }
    if (num_chars_on_left > 0) {
        this->print("\e[{}C", num_chars_on_left);
    }
    auto num_chars_on_right = this->input.get_distance_to_next('\n');
    if (num_chars_on_right > 0) {
        this->print(this->input.get().substr(this->input.get_cursor_pos(),
                                             num_chars_on_right));
        this->print("\e[{}D", num_chars_on_right);
    }
    return true;
}

bool CLI::insert(char c) {
    if (!this->input.insert(c)) {
        return false;
    }
    if (c == '\n') {
        this->print("\e[K\n\e[L");
        auto num_chars_on_right = this->input.get_distance_to_next('\n');
        if (num_chars_on_right > 0) {
            this->print(this->input.get().substr(this->input.get_cursor_pos(),
                                                 num_chars_on_right));
            this->print("\e[{}D", num_chars_on_right);
        }
        return true;
    }
    this->print("\e[@");
    this->print(c);
    return true;
}

} // namespace ln::shell
