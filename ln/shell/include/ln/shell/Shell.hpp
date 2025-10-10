#pragma once

#include "ln/shell/Input.hpp"
#include "ln/shell/Command.hpp"
#include "ln/stream.hpp"

#include "FreeRTOS/Addons/Clock.hpp"

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string_view>

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

class Shell {
public:
    struct Config {
        static constexpr std::size_t printfBufferSize = 256;
        static constexpr bool regularResponseIsEnabled = true;
        bool coloredOutput = true;
    } config;

    Shell(const char *strPromptLabel, ln::OutStream<char> &out_stream,
          Command *commandList = Command::globalCommandList);

    // NOTE: escape sequences are time sensitive !
    // TODO: move this to a dedicated uart receiver task and join by char queue
    // escape sequence finished (not time sensitive)
    bool putChar(const char &c);

    void print(const char &c, std::size_t timesToRepeat = 1);
    int print(const char *str, std::size_t timesToRepeat = 1);
    void printUnformatted(const char *pData, const std::size_t len, std::size_t timesToRepeat = 1);
    int printf(const char *fmt, ...);

    const Command *findCommand(std::size_t argcIn, const char *argvIn[], std::size_t &argCmdOffsetOut);

    Command::Result execute(const Command &command, std::size_t argc, const char *argv[],
                            const char *outputColorEscapeSequence = "\e[32m"); // default in green
    Command::Result execute(const Command &command, const char *strArgs = nullptr,
                            const char *outputColorEscapeSequence = "\e[33m");

private:
    bool lineFeed();

    bool handleEscape(const char &c);
    bool handleAnsiEscape(const char &c);
    bool handleAnsiDelimitedEscape(const char &c);
    bool handleAnsiDelimitedDelEscape(const char &c);
    bool deleteChar();
    bool onHomeKey();
    bool onArrowUpKey();
    bool onArrowDownKey();
    bool onArrowRightKey();
    bool onArrowLeftKey();

    bool backspaceChar();
    bool insertChar(const char &c);

    void promptNew();
    void printPrompt();

    enum class EscapeState : std::int8_t {
        failed = -1,
        none = 0,
        escaped,
        delimited,
        intermediate,
        finished,
    } escapeState;

    FreeRTOS::Addons::Clock::time_point escapeStartTime;

    Input input;
    bool isPrompted = true;
    const char *strPromptLabel;
    ln::OutStream<char> &out_stream;
    Command *commandList = nullptr;
};
} // namespace ln::shell
