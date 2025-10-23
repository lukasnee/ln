#pragma once

#include "ln/shell/Args.hpp"

#include <cstdint>
#include <cstddef>
#include <array>
#include <span>

namespace ln::shell {

class Input {
public:
    Input() = default;
    virtual ~Input() = default;

    void clear();

    std::span<char> get() { return std::span<char>(this->buf.data(), this->chars_used - 1); }

    bool is_full();
    bool is_empty();

    const char &get_char_at_cursor();
    const char *get_buffer_at_cursor(std::size_t &lengthOut);
    const char *get_buffer_at_base();

    bool is_cursor_on_base();
    bool is_cursor_on_end();

    bool set_cursor(std::size_t index);
    bool cursor_step_right();
    bool cursor_step_left();

    bool delete_char_at_cursor();
    bool backspace_char_at_cursor();
    bool insert_char(const char &c);

private:
    std::array<char, 256> buf{};
    std::size_t cursor_idx = 0;
    std::size_t chars_used = 1;
};

} // namespace ln::shell