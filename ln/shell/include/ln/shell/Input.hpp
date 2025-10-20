#pragma once

#include "ln/shell/Args.hpp"

#include <cstdint>
#include <cstddef>
#include <array>

namespace ln::shell {

class Input {
public:
    Input();
    ~Input() = default;

    void reset();

    bool isFull();
    bool isEmpty();

    const char &getCharAtCursor();
    const char *getBufferAtCursor(std::size_t &lengthOut);
    const char *getBufferAtBase();

    bool isCursorOnBase();
    bool isCursorOnEnd();

    bool setCursor(std::size_t index);
    bool cursorStepRight();
    bool cursorStepLeft();

    bool deleteCharAtCursor();
    bool backspaceCharAtCursor();
    bool insertChar(const char &c);

private:
    std::array<char, 256> args_buf{};

public:
    Args args;

private:
    std::size_t cursorIdx = 0;
    std::size_t charsUsed = 1;
};

} // namespace ln::shell