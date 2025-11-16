#pragma once

#include "ln/shell/Args.hpp"

#include <cstdint>
#include <cstddef>
#include <array>
#include <string_view>

namespace ln::shell {

class Input {
public:
    Input() = default;
    virtual ~Input() = default;

    void clear();

    std::string_view get() { return std::string_view{this->buf.data(), this->chars_used}; }

    size_t get_cursor_pos() { return this->cursor_idx; }

    bool is_full();
    bool is_empty();

    bool is_cursor_on_base();
    bool is_cursor_on_end();

    bool step_right();
    bool step_left();

    bool delete_char();
    bool backspace_char();
    bool insert_char(const char &c);

private:
    std::array<char, 256> buf{};
    std::size_t cursor_idx = 0;
    std::size_t chars_used = 0;
};

} // namespace ln::shell