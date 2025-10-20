#include "ln/shell/Input.hpp"

#include <cstring>

namespace ln::shell {
Input::Input() : args{this->args_buf} { this->reset(); }

void Input::reset() {
    this->args.clear();
    this->charsUsed = 1;
    this->cursorIdx = 0;
}

bool Input::isCursorOnBase() { return (this->cursorIdx == 0); }

bool Input::isCursorOnEnd() { return (this->charsUsed - this->cursorIdx == 1); }

bool Input::isEmpty() { return (this->isCursorOnBase() and this->isCursorOnEnd()); }

bool Input::isFull() { return (this->charsUsed >= this->args_buf.size()); }

const char &Input::getCharAtCursor() { return this->args_buf.at(this->cursorIdx); }

bool Input::setCursor(size_t index) {
    bool result = false;

    if (index < this->args_buf.size()) {
        this->cursorIdx = index;
        result = true;
    }

    return result;
}

bool Input::cursorStepRight() {
    bool result = false;

    if (!this->isFull()) {
        this->cursorIdx++;
        result = true;
    }

    return result;
}

bool Input::cursorStepLeft() {
    bool result = false;

    if (!this->isCursorOnBase()) {
        this->cursorIdx--;
        result = true;
    }

    return result;
}

bool Input::deleteCharAtCursor() {
    bool result = false;

    if (!this->isEmpty() && !this->isCursorOnEnd()) {
        std::memmove(&this->args_buf[this->cursorIdx], &this->args_buf[this->cursorIdx + 1],
                     this->charsUsed - (this->cursorIdx + 1));
        this->args_buf[--this->charsUsed] = '\0';
        result = true;
    }

    return result;
}

const char *Input::getBufferAtCursor(std::size_t &lengthOut) {
    lengthOut = this->charsUsed - this->cursorIdx;
    return &this->args_buf[this->cursorIdx];
}

const char *Input::getBufferAtBase() { return &this->args_buf[0]; }

bool Input::backspaceCharAtCursor() {
    bool result = false;

    if (false == this->isCursorOnBase()) {
        std::memmove(&this->args_buf[this->cursorIdx - 1], &this->args_buf.at(this->cursorIdx),
                     this->charsUsed - this->cursorIdx);
        if (this->cursorStepLeft()) {
            this->args_buf[--this->charsUsed] = '\0';
            result = true;
        }
    }

    return result;
}

bool Input::insertChar(const char &c) {
    bool result = false;

    if (!this->isFull()) {
        if (!this->isCursorOnEnd()) {
            std::memmove(&this->args_buf[this->cursorIdx + 1], &this->args_buf.at(this->cursorIdx),
                         this->charsUsed - this->cursorIdx);
        }

        this->args_buf[this->cursorIdx] = c;
        this->cursorStepRight();
        this->args_buf[this->charsUsed++] = '\0';
        result = true;
    }
    return result;
}
} // namespace ln::shell
