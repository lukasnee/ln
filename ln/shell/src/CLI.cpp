#include "ln/shell/CLI.hpp"
// TODO: make arrow up repeat buffer
// TODO: some kind of esacpe signal mechanism to inform running cmd to exit.

#include <cstring>
#include <type_traits>

namespace ln::shell {
CLI::CLI(ln::OutStream<char> &out_stream, Cmd *cmd_list) : out_stream(out_stream), cmd_list(cmd_list){};

void CLI::print(const char &c, std::size_t times_to_repeat) {
    while (times_to_repeat--) {
        this->out_stream.put(c);
    }
}

void CLI::print(const char *data, const std::size_t size) {
    std::size_t size_left = size;
    const char *data_it = data;
    while (size_left--) {
        this->print(*(data_it++));
    }
}

int CLI::print(const char *str) {
    int chars_printed = 0;
    for (const char *c = str; *c != '\0'; ++c) {
        this->print(*c); // TODO print whole sentence not char by char !
        chars_printed++;
    }
    return chars_printed;
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

std::tuple<const Cmd *, std::size_t> CLI::find_cmd(std::size_t argc, const char *argv[]) {
    const Cmd *cmd = this->cmd_list;
    if (!cmd) {
        return {nullptr, 0};
    }
    if (argc == 0 || !argv[0]) {
        return {cmd, 0};
    }
    std::size_t arg_offset = 0;
    cmd = cmd->find_neighbour_cmd(argv[arg_offset]);
    if (!cmd) {
        return {nullptr, 0};
    }
    while (argc - arg_offset - 1) {
        const Cmd *subcmd = cmd->find_subcmd(argv[arg_offset + 1]);
        if (!subcmd) {
            break;
        }
        arg_offset++;
        cmd = subcmd;
        continue;
    }

    return {cmd, arg_offset};
}

Err CLI::execute(const Cmd &cmd, std::size_t argc, const char *argv[], const char *output_color_escape_sequence) {
    if (!cmd.function) {
        this->print("\e[31mcommand has no method\n"); // red
        return Err::unexpected;
    }
    this->print(output_color_escape_sequence); // response in green
    Err err = cmd.function(Cmd::Ctx{*this, argc, argv});
    if (!Config::regular_response_is_enabled) {
        return err;
    }
    if (err == Err::ok) {
        this->print("\n" ANSI_COLOR_GREEN "OK");
    }
    else if (static_cast<std::int8_t>(err) < 0) {
        this->printf("\n" ANSI_COLOR_RED "FAIL: %d", static_cast<std::underlying_type_t<decltype(err)>>(err));
    }
    else if (err == Err::okQuiet) {
        /* nothin */
    }
    this->print(ANSI_COLOR_RESET "\n");
    return err;
}

Err CLI::execute(const Cmd &cmd, const char *arg_string, const char *output_color_escape_sequence) {
    std::array<char, 256> args_buf;
    Args args(args_buf, arg_string);
    return this->execute(cmd, args.get_argc(), args.get_argv(), output_color_escape_sequence);
}

/** @return true if sequence finished */
bool CLI::put_char(const char &c) {
    if (this->handle_escape(c)) {
        return false;
    }
    if (c == '\b') {
        this->backspace_char();
        return true;
    }
    if (' ' <= c && c <= '~') {
        if (this->is_prompted) {
            this->input.reset();
            this->is_prompted = false;
        }
        this->insert_char(c);
        return true;
    }
    if (c == '\r') {
        if (this->is_prompted) {
            this->input.reset();
        }
        this->line_feed();
        return true;
    }
    return false;
}

bool CLI::line_feed() {
    this->print("\n");
    if (!this->input.args.resolve_into_args()) {
        return false;
    }
    const auto [cmd, cmd_arg_offset] = this->find_cmd(this->input.args.get_argc(), this->input.args.get_argv());
    if (!cmd) {
        this->print("\e[39mcommand not found\n");
        this->prompt_new();
        return false;
    }
    this->execute(*cmd, this->input.args.get_argc() - cmd_arg_offset, this->input.args.get_argv() + cmd_arg_offset);
    this->prompt_new();
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
    if (c == 0x7F) // DELete
    {
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
    if (!this->input.delete_char_at_cursor()) {
        return false;
    }
    std::size_t string_at_cursor_length;
    const char *string_at_cursor = this->input.get_buffer_at_cursor(string_at_cursor_length);
    this->print(string_at_cursor, string_at_cursor_length + 1);
    this->print("  ");
    this->print('\b', string_at_cursor_length + 1);
    return true;
}

bool CLI::on_home_key() {
    while (this->on_arrow_left_key()) {
    }
    return true;
}

bool CLI::on_arrow_up_key() {
    if (!this->input.args.restore_into_string()) {
        return false;
    }
    int chars_printed = this->printf(this->input.get_buffer_at_base());
    if (chars_printed <= 0) {
        return false;
    }
    if (!this->input.set_cursor(chars_printed)) {
        return false;
    }
    return true;
}

bool CLI::on_arrow_down_key() {
    this->input.reset();
    this->is_prompted = true;
    return true;
}

bool CLI::on_arrow_left_key() {
    if (!this->input.cursor_step_left()) {
        return false;
    }
    this->print('\b');
    return true;
}

bool CLI::on_arrow_right_key() {
    if (!this->input.cursor_step_right()) {
        return false;
    }
    std::size_t length;
    this->print(*(this->input.get_buffer_at_cursor(length) - 1));
    return true;
}

void CLI::prompt_new(void) {
    this->is_prompted = true;
    this->print_prompt();
}

void CLI::print_prompt(void) { this->print(ANSI_COLOR_BLUE "> " ANSI_COLOR_YELLOW); }

/** @return true if actually backspaced */
bool CLI::backspace_char() {
    if (!this->input.backspace_char_at_cursor()) {
        return false;
    }
    std::size_t string_at_cursor_length;
    const char *string_at_cursor = this->input.get_buffer_at_cursor(string_at_cursor_length);
    this->print('\b');
    this->print(string_at_cursor, string_at_cursor_length);
    this->print(' ');
    for (std::size_t i = string_at_cursor_length; i > 0; i--) {
        this->print('\b');
    }
    return true;
}

/** @return true if actually inserted */
bool CLI::insert_char(const char &c) {
    if (!this->input.insert_char(c)) {
        return false;
    }
    if (this->input.is_cursor_on_end()) {
        /* append */
        this->print(c);
        return true;
    }
    /* insert in middle */
    std::size_t length;
    this->print(c);
    this->print(this->input.get_buffer_at_cursor(length));
    this->print('\b', length - 1);
    return true;
}

} // namespace ln::shell
