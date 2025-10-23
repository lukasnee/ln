#include "ln/shell/Input.hpp"

#include <cstring>

namespace ln::shell {

void Input::clear() {
    this->chars_used = 1;
    this->cursor_idx = 0;
}

bool Input::is_cursor_on_base() { return (this->cursor_idx == 0); }

bool Input::is_cursor_on_end() { return (this->chars_used - this->cursor_idx == 1); }

bool Input::is_empty() { return (this->is_cursor_on_base() and this->is_cursor_on_end()); }

bool Input::is_full() { return (this->chars_used >= this->buf.size()); }

const char &Input::get_char_at_cursor() { return this->buf.at(this->cursor_idx); }

bool Input::set_cursor(size_t index) {
    if (index >= this->buf.size()) {
        return false;
    }
    this->cursor_idx = index;
    return true;
}

bool Input::cursor_step_right() {
    if (this->is_full()) {
        return false;
    }
    this->cursor_idx++;
    return true;
}

bool Input::cursor_step_left() {
    if (this->is_cursor_on_base()) {
        return false;
    }
    this->cursor_idx--;
    return true;
}

bool Input::delete_char_at_cursor() {
    if (this->is_empty() || this->is_cursor_on_end()) {
        return false;
    }
    std::memmove(&this->buf[this->cursor_idx], &this->buf[this->cursor_idx + 1],
                 this->chars_used - (this->cursor_idx + 1));
    this->buf[--this->chars_used] = '\0';
    return true;
}

const char *Input::get_buffer_at_cursor(std::size_t &length_out) {
    length_out = this->chars_used - this->cursor_idx;
    return &this->buf[this->cursor_idx];
}

const char *Input::get_buffer_at_base() { return &this->buf[0]; }

bool Input::backspace_char_at_cursor() {
    if (this->is_cursor_on_base()) {
        return false;
    }
    std::memmove(&this->buf[this->cursor_idx - 1], &this->buf.at(this->cursor_idx),
                 this->chars_used - this->cursor_idx);
    if (!this->cursor_step_left()) {
        return false;
    }
    this->buf[--this->chars_used] = '\0';
    return true;
}

bool Input::insert_char(const char &c) {
    if (this->is_full()) {
        return false;
    }
    if (this->is_cursor_on_end()) {
        std::memmove(&this->buf[this->cursor_idx + 1], &this->buf.at(this->cursor_idx),
                     this->chars_used - this->cursor_idx);
    }
    this->buf[this->cursor_idx] = c;
    this->cursor_step_right();
    this->buf[this->chars_used++] = '\0';
    return true;
}

} // namespace ln::shell
