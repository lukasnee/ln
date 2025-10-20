#include "ln/shell/CLI.hpp"
// TODO: make arrow up repeat buffer
// TODO: some kind of esacpe signal mechanism to inform running cmd to exit.

#include <cstring>

namespace ln::shell {
CLI::CLI(ln::OutStream<char> &out_stream, Cmd *cmd_list) : out_stream(out_stream), cmd_list(cmd_list){};

void CLI::print(const char &c, std::size_t times_to_repeat) {
    while (times_to_repeat--) {
        this->out_stream.put(c);
    }
}

void CLI::print_unformatted(const char *data, const std::size_t len, std::size_t times_to_repeat) {
    while (times_to_repeat--) {
        std::size_t len_left = len;
        const char *data_it = data;

        while (len_left--) {
            this->print(*(data_it++));
        }
    }
}

int CLI::print(const char *string, std::size_t times_to_repeat) {
    int chars_printed = 0;

    while (times_to_repeat--) {
        for (const char *c = string; *c != '\0'; ++c) {
            this->print(*c); // TODO print whole sentence not char by char !
            chars_printed++;
        }
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
    Err result = Err::unknown;

    if (cmd.function == nullptr) {
        this->print("\e[31mcommand has no method\n"); // red
    }
    else {
        this->print(output_color_escape_sequence); // response in green
        result = cmd.function(Cmd::Ctx{*this, argc, argv});

        if (Config::regular_response_is_enabled) {
            if (result == Err::ok) {
                this->print("\n" ANSI_COLOR_GREEN "OK");
            }
            else if (static_cast<std::int8_t>(result) < 0) {
                this->printf("\n" ANSI_COLOR_RED "FAIL: %d", static_cast<std::int8_t>(result));
            }
            else if (result == Err::okQuiet) {
                /* nothin */
            }
            this->print(ANSI_COLOR_RESET "\n");
        }
    }

    return result;
}

Err CLI::execute(const Cmd &cmd, const char *arg_string, const char *output_color_escape_sequence) {
    std::array<char, 256> args_buf;
    Args args(args_buf, arg_string);
    return this->execute(cmd, args.get_argc(), args.get_argv(), output_color_escape_sequence);
}

/** @return true if sequence finished */
bool CLI::put_char(const char &c) {
    bool result = false;

    if (this->handle_escape(c)) {
    }
    else if (c == '\b') {
        result = true;
        this->backspace_char();
    }
    else if (' ' <= c && c <= '~') {
        result = true;

        if (this->is_prompted) {
            this->input.reset();
            this->is_prompted = false;
        }
        this->insert_char(c);
    }
    else if (c == '\r') {
        result = true;

        if (this->is_prompted) {
            this->input.reset();
        }
        this->line_feed();
    }
    return result;
}

bool CLI::line_feed() {
    bool result = false;

    this->print("\n");

    if (this->input.args.resolve_into_args()) {

        const auto [cmd, cmd_arg_offset] = this->find_cmd(this->input.args.get_argc(), this->input.args.get_argv());
        if (!cmd) {
            this->print("\e[39mcommand not found\n");
            result = false;
        }
        else {
            this->execute(*cmd, this->input.args.get_argc() - cmd_arg_offset,
                          this->input.args.get_argv() + cmd_arg_offset);
            result = true;
        }
    }

    this->prompt_new();

    return result;
}

/** @result false - nothing to handle */
bool CLI::handle_escape(const char &c) {
    bool result = false;

    if (c == '\e') {
        this->escape_start_time = FreeRTOS::Addons::Clock::now();
        this->escape_state = EscapeState::escaped;
        result = true;
    }
    else if (this->escape_state == EscapeState::escaped || this->escape_state == EscapeState::delimited ||
             this->escape_state == EscapeState::intermediate || this->escape_state == EscapeState::finished) {
        if (FreeRTOS::Addons::Clock::now() - this->escape_start_time > std::chrono::milliseconds(2)) {
            /* timed out */
            this->escape_state = EscapeState::none;
        }
        else if (c == 0x7F) // DELete
        {
            delete_char();
            result = true;
            this->escape_state = EscapeState::finished;
        }
        else {
            result = this->handle_ansi_escape(c);
        }
    }
    else {
        this->escape_state = EscapeState::failed;
    }

    if (this->escape_state == EscapeState::failed || this->escape_state == EscapeState::finished) {
        this->escape_state = EscapeState::none;
    }

    return result;
}

/** @result false - nothing to handle */
bool CLI::handle_ansi_escape(const char &c) {
    bool result = false;

    if (c == '[') /* open delimiter */
    {
        this->escape_state = EscapeState::delimited;
        result = true;
    }
    else if (this->escape_state == EscapeState::delimited || this->escape_state == EscapeState::intermediate ||
             this->escape_state == EscapeState::finished) {
        result = this->handle_ansi_delimited_escape(c);
    }
    else {
        this->escape_state = EscapeState::failed;
    }

    return result;
}

bool CLI::handle_ansi_delimited_escape(const char &c) {
    bool result = false;

    if (this->handle_ansi_delimited_del_escape(c)) {
        result = true;
    }
    else if (c == 'H') {
        this->on_home_key();
        result = true;
        this->escape_state = EscapeState::finished;
    }
    else if (c == 'A') {
        this->on_arrow_up_key();
        result = true;
        this->escape_state = EscapeState::finished;
    }
    else if (c == 'B') {
        this->on_arrow_down_key();
        result = true;
        this->escape_state = EscapeState::finished;
    }
    else if (c == 'C') {
        this->on_arrow_right_key();
        result = true;
        this->escape_state = EscapeState::finished;
    }
    else if (c == 'D') {
        this->on_arrow_left_key();
        result = true;
        this->escape_state = EscapeState::finished;
    }
    else {
        this->escape_state = EscapeState::failed;
    }

    return result;
}

bool CLI::handle_ansi_delimited_del_escape(const char &c) {
    bool result = false;

    if ((this->escape_state == EscapeState::delimited || this->escape_state == EscapeState::intermediate ||
         this->escape_state == EscapeState::finished) &&
        c == '3') {
        this->escape_state = EscapeState::intermediate;
        result = true;
    }
    else if ((this->escape_state == EscapeState::intermediate || this->escape_state == EscapeState::finished) &&
             c == '~') {
        this->delete_char();
        this->escape_state = EscapeState::finished;
        result = true;
    }
    else {
        this->escape_state = EscapeState::failed;
    }

    return result;
}

bool CLI::delete_char() {
    bool result = false;

    if (this->input.delete_char_at_cursor()) {
        std::size_t string_at_cursor_length;
        const char *string_at_cursor = this->input.get_buffer_at_cursor(string_at_cursor_length);
        this->print_unformatted(string_at_cursor, string_at_cursor_length + 1);
        this->print("  ");
        this->print('\b', string_at_cursor_length + 1);
        result = true;
    }

    return result;
}

bool CLI::on_home_key() {
    while (this->on_arrow_left_key()) {
    }
    return true;
}

bool CLI::on_arrow_up_key() {
    bool result = false;

    if (this->input.args.restore_into_string()) {
        int chars_printed = this->printf(this->input.get_buffer_at_base());
        if (chars_printed > 0) {
            if (this->input.set_cursor(chars_printed)) {
                result = true;
            }
        }
    }

    return result;
}

bool CLI::on_arrow_down_key() {
    this->input.reset();
    this->is_prompted = true;
    return true;
}

bool CLI::on_arrow_left_key() {
    bool result = false;

    if (this->input.cursor_step_left()) {
        this->print('\b');
        result = true;
    }

    return result;
}

bool CLI::on_arrow_right_key() {
    bool result = false;

    if (this->input.cursor_step_right()) {
        std::size_t length;
        this->print(*(this->input.get_buffer_at_cursor(length) - 1));
        result = true;
    }

    return result;
}

void CLI::prompt_new(void) {
    this->is_prompted = true;
    this->print_prompt();
}

void CLI::print_prompt(void) { this->print(ANSI_COLOR_BLUE "> " ANSI_COLOR_YELLOW); }

/** @return true if actually backspaced */
bool CLI::backspace_char() {
    bool result = false;

    if (this->input.backspace_char_at_cursor()) {
        std::size_t string_at_cursor_length;
        const char *string_at_cursor = this->input.get_buffer_at_cursor(string_at_cursor_length);
        this->print('\b');
        this->print_unformatted(string_at_cursor, string_at_cursor_length);
        this->print(' ');
        for (std::size_t i = string_at_cursor_length; i > 0; i--) {
            this->print('\b');
        }
        result = true;
    }

    return result;
}

/** @return true if actually inserted */
bool CLI::insert_char(const char &c) {
    bool result = false;

    if (this->input.insert_char(c)) {
        result = true;
        if (this->input.is_cursor_on_end()) {
            /* append */
            this->print(c);
        }
        else {
            /* insert in middle */
            std::size_t length;
            this->print(c);
            this->print(this->input.get_buffer_at_cursor(length));
            this->print('\b', length - 1);
        }
    }

    return result;
}

} // namespace ln::shell
