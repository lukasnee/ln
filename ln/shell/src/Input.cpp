/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/Input.hpp"

#include <cstring>

namespace ln::shell {

void Input::clear() {
    this->chars_used = 0;
    this->cursor_idx = 0;
}

bool Input::is_cursor_on_base() { return (this->cursor_idx == 0); }

bool Input::is_cursor_on_end() { return this->chars_used == this->cursor_idx; }

bool Input::is_empty() { return (this->is_cursor_on_base() && this->is_cursor_on_end()); }

bool Input::is_full() { return (this->chars_used == this->buf.size()); }

bool Input::step_right() {
    if (this->is_cursor_on_end()) {
        return false;
    }
    if (this->is_full()) {
        return false;
    }
    this->cursor_idx++;
    return true;
}

bool Input::step_left() {
    if (this->is_cursor_on_base()) {
        return false;
    }
    this->cursor_idx--;
    return true;
}

bool Input::delete_char() {
    if (this->is_empty() || this->is_cursor_on_end()) {
        return false;
    }
    std::memmove(&this->buf[this->cursor_idx], &this->buf[this->cursor_idx + 1],
                 this->chars_used - (this->cursor_idx + 1));
    this->chars_used--;
    return true;
}

bool Input::backspace_char() {
    if (this->is_cursor_on_base()) {
        return false;
    }
    if (!this->is_cursor_on_end()) {
        std::memmove(&this->buf[this->cursor_idx - 1], &this->buf.at(this->cursor_idx),
                     this->chars_used - this->cursor_idx + 1);
    }
    this->cursor_idx--;
    this->chars_used--;
    return true;
}

bool Input::insert_char(const char &c) {
    if (this->is_full()) {
        return false;
    }
    std::memmove(&this->buf[this->cursor_idx + 1], &this->buf.at(this->cursor_idx),
                 this->chars_used - this->cursor_idx);
    this->buf[this->cursor_idx] = c;
    this->cursor_idx++;
    this->chars_used++;
    return true;
}

} // namespace ln::shell
