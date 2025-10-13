#include "ln/shell/CLI.hpp"
// TODO: make arrow up repeat buffer
// TODO: some kind of esacpe signal mechanism to inform running cmd to exit.

#include <cstring>

namespace ln::shell {
CLI::CLI(const char *strPromptLabel, ln::OutStream<char> &out_stream, Cmd *commandList)
    : strPromptLabel(strPromptLabel), out_stream(out_stream), commandList(commandList){};

void CLI::print(const char &c, std::size_t timesToRepeat) {
    while (timesToRepeat--) {
        this->out_stream.put(c);
    }
}

void CLI::printUnformatted(const char *pData, const std::size_t len, std::size_t timesToRepeat) {
    while (timesToRepeat--) {
        std::size_t lenIt = len;
        const char *pDataIt = pData;

        while (lenIt--) {
            this->print(*(pDataIt++));
        }
    }
}

int CLI::print(const char *string, std::size_t timesToRepeat) {
    int charsPrinted = 0;

    while (timesToRepeat--) {
        for (const char *c = string; *c != '\0'; ++c) {
            this->print(*c); // TODO print whole sentence not char by char !
            charsPrinted++;
        }
    }

    return charsPrinted;
}

int CLI::printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    std::array<char, Config::printfBufferSize> txBuffer;
    vsnprintf(txBuffer.data(), txBuffer.size(), fmt, args);
    int charsPrinted = this->print(txBuffer.data());

    va_end(args);

    return charsPrinted;
}

std::tuple<const Cmd *, std::size_t> CLI::findCommand(std::size_t argcIn, const char *argvIn[]) {
    const Cmd *pCommand = this->commandList;

    std::size_t arg_offset = 0;

    if (pCommand) {
        if (argcIn && argvIn[0]) {
            pCommand = pCommand->findNeighbourCommand(argvIn[arg_offset]);
            if (pCommand) {
                while (argcIn - arg_offset - 1) {
                    const Cmd *pSubcommand = pCommand->findSubcommand(argvIn[arg_offset + 1]);
                    if (pSubcommand) {
                        arg_offset++;
                        pCommand = pSubcommand;
                        continue;
                    }
                    else {
                        break;
                    }
                }
            }
        }
    }

    return {pCommand, arg_offset};
}

Err CLI::execute(const Cmd &cmd, std::size_t argc, const char *argv[], const char *outputColorEscapeSequence) {
    Err result = Err::unknown;

    if (cmd.function == nullptr) {
        this->print("\e[31mcommand has no method\n"); // red
    }
    else {
        this->print(outputColorEscapeSequence); // response in green
        result = cmd.function(Cmd::Ctx{*this, argc, argv});

        if (Config::regularResponseIsEnabled) {
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

Err CLI::execute(const Cmd &cmd, const char *argString, const char *outputColorEscapeSequence) {
    ArgBuffer argBuffer(argString);
    return this->execute(cmd, argBuffer.getArgc(), argBuffer.getArgv(), outputColorEscapeSequence);
}

/** @return true if sequence finished */
bool CLI::putChar(const char &c) {
    bool result = false;

    if (this->handleEscape(c)) {
    }
    else if (c == '\b') {
        result = true;
        this->backspaceChar();
    }
    else if (' ' <= c && c <= '~') {
        result = true;

        if (this->isPrompted) {
            this->input.reset();
            this->isPrompted = false;
        }
        this->insertChar(c);
    }
    else if (c == '\r') {
        result = true;

        if (this->isPrompted) {
            this->input.reset();
        }
        this->lineFeed();
    }
    return result;
}

bool CLI::lineFeed() {
    bool result = false;

    this->print("\n");

    if (this->input.resolveIntoArgs()) {

        const auto [pCommand, cmd_arg_offset] = this->findCommand(this->input.getArgc(), this->input.getArgv());
        if (!pCommand) {
            this->print("\e[39mcommand not found\n");
            result = false;
        }
        else {
            this->execute(*pCommand, this->input.getArgc() - cmd_arg_offset, this->input.getArgv() + cmd_arg_offset);
            result = true;
        }
    }

    this->promptNew();

    return result;
}

/** @result false - nothing to handle */
bool CLI::handleEscape(const char &c) {
    bool result = false;

    if (c == '\e') {
        this->escapeStartTime = FreeRTOS::Addons::Clock::now();
        this->escapeState = EscapeState::escaped;
        result = true;
    }
    else if (this->escapeState == EscapeState::escaped || this->escapeState == EscapeState::delimited ||
             this->escapeState == EscapeState::intermediate || this->escapeState == EscapeState::finished) {
        if (FreeRTOS::Addons::Clock::now() - this->escapeStartTime > std::chrono::milliseconds(2)) {
            /* timed out */
            this->escapeState = EscapeState::none;
        }
        else if (c == 0x7F) // DELete
        {
            deleteChar();
            result = true;
            this->escapeState = EscapeState::finished;
        }
        else {
            result = this->handleAnsiEscape(c);
        }
    }
    else {
        this->escapeState = EscapeState::failed;
    }

    if (this->escapeState == EscapeState::failed || this->escapeState == EscapeState::finished) {
        this->escapeState = EscapeState::none;
    }

    return result;
}

/** @result false - nothing to handle */
bool CLI::handleAnsiEscape(const char &c) {
    bool result = false;

    if (c == '[') /* open delimiter */
    {
        this->escapeState = EscapeState::delimited;
        result = true;
    }
    else if (this->escapeState == EscapeState::delimited || this->escapeState == EscapeState::intermediate ||
             this->escapeState == EscapeState::finished) {
        result = this->handleAnsiDelimitedEscape(c);
    }
    else {
        this->escapeState = EscapeState::failed;
    }

    return result;
}

bool CLI::handleAnsiDelimitedEscape(const char &c) {
    bool result = false;

    if (this->handleAnsiDelimitedDelEscape(c)) {
        result = true;
    }
    else if (c == 'H') {
        this->onHomeKey();
        result = true;
        this->escapeState = EscapeState::finished;
    }
    else if (c == 'A') {
        this->onArrowUpKey();
        result = true;
        this->escapeState = EscapeState::finished;
    }
    else if (c == 'B') {
        this->onArrowDownKey();
        result = true;
        this->escapeState = EscapeState::finished;
    }
    else if (c == 'C') {
        this->onArrowRightKey();
        result = true;
        this->escapeState = EscapeState::finished;
    }
    else if (c == 'D') {
        this->onArrowLeftKey();
        result = true;
        this->escapeState = EscapeState::finished;
    }
    else {
        this->escapeState = EscapeState::failed;
    }

    return result;
}

bool CLI::handleAnsiDelimitedDelEscape(const char &c) {
    bool result = false;

    if ((this->escapeState == EscapeState::delimited || this->escapeState == EscapeState::intermediate ||
         this->escapeState == EscapeState::finished) &&
        c == '3') {
        this->escapeState = EscapeState::intermediate;
        result = true;
    }
    else if ((this->escapeState == EscapeState::intermediate || this->escapeState == EscapeState::finished) &&
             c == '~') {
        this->deleteChar();
        this->escapeState = EscapeState::finished;
        result = true;
    }
    else {
        this->escapeState = EscapeState::failed;
    }

    return result;
}

bool CLI::deleteChar() {
    bool result = false;

    if (this->input.deleteCharAtCursor()) {
        std::size_t stringAtCursorLength;
        const char *stringAtCursor = this->input.getBufferAtCursor(stringAtCursorLength);
        this->printUnformatted(stringAtCursor, stringAtCursorLength + 1);
        this->print("  ");
        this->print('\b', stringAtCursorLength + 1);
        result = true;
    }

    return result;
}

bool CLI::onHomeKey() {
    while (this->onArrowLeftKey()) {
    }
    return true;
}

bool CLI::onArrowUpKey() {
    bool result = false;

    if (this->input.restoreIntoString()) {
        int charsPrinted = this->printf(this->input.getBufferAtBase());
        if (charsPrinted > 0) {
            if (this->input.setCursor(charsPrinted)) {
                result = true;
            }
        }
    }

    return result;
}

bool CLI::onArrowDownKey() {
    this->input.reset();
    this->isPrompted = true;
    return true;
}

bool CLI::onArrowLeftKey() {
    bool result = false;

    if (this->input.cursorStepLeft()) {
        this->print('\b');
        result = true;
    }

    return result;
}

bool CLI::onArrowRightKey() {
    bool result = false;

    if (this->input.cursorStepRight()) {
        std::size_t length;
        this->print(*(this->input.getBufferAtCursor(length) - 1));
        result = true;
    }

    return result;
}

void CLI::promptNew(void) {
    this->isPrompted = true;
    this->printPrompt();
}

void CLI::printPrompt(void) { this->print(this->strPromptLabel); }

/** @return true if actually backspaced */
bool CLI::backspaceChar() {
    bool result = false;

    if (this->input.backspaceCharAtCursor()) {
        std::size_t stringAtCursorLength;
        const char *stringAtCursor = this->input.getBufferAtCursor(stringAtCursorLength);
        this->print('\b');
        this->printUnformatted(stringAtCursor, stringAtCursorLength);
        this->print(' ');
        for (std::size_t i = stringAtCursorLength; i > 0; i--) {
            this->print('\b');
        }
        result = true;
    }

    return result;
}

/** @return true if actually inserted */
bool CLI::insertChar(const char &c) {
    bool result = false;

    if (this->input.insertChar(c)) {
        result = true;
        if (this->input.isCursorOnEnd()) {
            /* append */
            this->print(c);
        }
        else {
            /* insert in middle */
            std::size_t length;
            this->print(c);
            this->print(this->input.getBufferAtCursor(length));
            this->print('\b', length - 1);
        }
    }

    return result;
}
} // namespace ln::shell
