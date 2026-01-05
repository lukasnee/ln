/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/History.hpp"
#include "ln/shell/CLI.hpp"

#include "ln/logger/logger.h"
#include <string_view>
#include <ranges>
#include <algorithm>

LOG_MODULE(cli_history, LOGGER_LEVEL_INFO);

namespace ln::shell {

std::ranges::subrange<ln::RingBufferView<char>::iterator> History::get_current_recall_line() {
    auto range = std::ranges::subrange{this->recall_pos, this->ring_buffer.end()};
    return std::ranges::subrange{this->recall_pos, std::ranges::find(range, '\n')};
}

void History::add_line(std::string_view line) {
    if (!this->ring_buffer.push_overwrite(line)) {
        LN_PANIC();
        return;
    }
    if (!this->ring_buffer.push_overwrite('\n')) {
        LN_PANIC();
        return;
    }
    this->recall_pos = this->ring_buffer.end();
}

std::ranges::subrange<ln::RingBufferView<char>::iterator> History::recall_previous() {
    auto line_begin_it = this->ring_buffer.begin();
    auto line_end_it = this->recall_pos;
    auto range = std::ranges::subrange{line_begin_it, line_end_it} | std::views::reverse;
    if (auto it = std::ranges::find(range, '\n'); it != std::ranges::end(range)) {
        line_end_it = std::prev(it.base());
    }
    range = std::ranges::subrange{line_begin_it, line_end_it} | std::views::reverse;
    auto it = std::ranges::find(range, '\n');
    this->recall_pos = it.base();
    return std::ranges::subrange{this->recall_pos, line_end_it};
}

std::ranges::subrange<ln::RingBufferView<char>::iterator> History::recall_next() {
    auto line_begin_it = this->recall_pos;
    auto line_end_it = this->ring_buffer.end();
    auto range = std::ranges::subrange{line_begin_it, line_end_it};
    if (auto it = std::ranges::find(range, '\n'); it != std::ranges::end(range)) {
        line_begin_it = std::next(it);
    }
    range = std::ranges::subrange{line_begin_it, line_end_it};
    auto it = std::ranges::find(range, '\n');
    if (it != std::ranges::end(range)) {
        this->recall_pos = line_begin_it;
    }
    return std::ranges::subrange{line_begin_it, it};
}

Err History::cmd_history_fn(Cmd::Ctx ctx) {
    for (const char c : ctx.cli.history.ring_buffer) {
        ctx.cli.print(c);
    }
    return Err::ok;
};

Cmd History::cmd_history = Cmd{Cmd::Cfg{.cmd_list = Cmd::base_cmd_list,
                                        .name = "history,hist",
                                        .short_description = "print command history",
                                        .fn = [](Cmd::Ctx ctx) {
                                            for (const char c : ctx.cli.history.ring_buffer) {
                                                ctx.cli.print(c);
                                            }
                                            return Err::ok;
                                        }}};

} // namespace ln::shell
