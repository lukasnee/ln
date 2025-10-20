#pragma once

#include "ln/shell/Input.hpp"
#include "ln/shell/Cmd.hpp"
#include "ln/stream.hpp"

#include "FreeRTOS/Addons/Clock.hpp"

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <tuple>

using namespace std::string_view_literals;

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
        static constexpr std::size_t printf_buffer_size = 256;
        static constexpr bool regular_response_is_enabled = true;
        bool colored_output = true;
    } config;

    CLI(ln::OutStream<char> &out_stream, Cmd *cmd_list = Cmd::global_command_list);

    // NOTE: escape sequences are time sensitive !
    // TODO: move this to a dedicated uart receiver task and join by char queue
    // escape sequence finished (not time sensitive)
    bool put_char(const char &c);

    void print(const char &c, std::size_t times_to_repeat = 1);
    int print(const char *str, std::size_t times_to_repeat = 1);
    void print_unformatted(const char *data, const std::size_t len, std::size_t times_to_repeat = 1);
    int printf(const char *fmt, ...);

    std::tuple<const Cmd *, std::size_t> find_cmd(std::size_t argcIn, const char *argvIn[]);

    Err execute(const Cmd &cmd, std::size_t argc, const char *argv[],
                const char *output_color_escape_sequence = "\e[32m"); // default in green
    Err execute(const Cmd &cmd, const char *str_args = nullptr, const char *output_color_escape_sequence = "\e[33m");

private:
    bool line_feed();

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
    bool insert_char(const char &c);

    void prompt_new();
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
    bool is_prompted = true;
    ln::OutStream<char> &out_stream;
    Cmd *cmd_list = nullptr;
};
} // namespace ln::shell
