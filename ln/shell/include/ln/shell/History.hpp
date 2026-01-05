/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "ln/RingBuffer.hpp"
#include "ln/shell/Cmd.hpp"

#include <span>

namespace ln::shell {

class History {

public:
    History(std::span<char> history_buf) : ring_buffer(history_buf) {}

    void add_line(std::string_view line);
    std::ranges::subrange<ln::RingBufferView<char>::iterator> get_current_recall_line();
    std::ranges::subrange<ln::RingBufferView<char>::iterator> recall_previous();
    std::ranges::subrange<ln::RingBufferView<char>::iterator> recall_next();

private:
    ln::RingBufferView<char> ring_buffer;
    ln::RingBufferView<char>::iterator recall_pos = ring_buffer.end();
    bool last_recall_was_matching = false;
};

} // namespace ln::shell
