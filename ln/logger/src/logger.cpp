// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/logger/logger.h"
#include "ln/ln.h"
#include "ln/Clock.hpp"

#include <fmt/core.h>

#include <cstdio>

namespace ln::logger {

Logger &Logger::get_instance() {
    static Logger instance;
    return instance;
};

Logger &get_instance() { return Logger::get_instance(); }

void Logger::enable() {
    auto config = this->config;
    config.enabled_run_time = true;
    this->set_config(config);
}

void Logger::flush_buffer() {
    if constexpr (!Config::enabled_compile_time) {
        return;
    }
    if (ln::interrupt_context()) {
        LN_PANIC();
    }
    LockGuard lock_guard(this->mutex);
    if (!this->config.enabled_run_time) {
        return;
    }
    this->_flush_buffer();
}

void Logger::set_level(Level log_level) { this->config.log_level = log_level; }

bool Logger::set_config(const Config &config) {
    // TODO: validation here
    this->config = config;
    return true;
}

Module::Module(const std::string_view name, Level log_level, Logger &logger)
    : name(name), log_level(log_level), logger(logger) {}

void Module::set_level(Level log_level) { this->log_level = log_level; }

int Logger::print_header(const Module &module, const Level &level) {

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

    struct LevelDescr {
        std::string_view tag_name;
        std::string_view color;
    };
    static constexpr LevelDescr level_descrs[5] = {{"DBG", ANSI_COLOR_MAGENTA},
                                                   {"INF", ANSI_COLOR_DEFAULT},
                                                   {"WRN", ANSI_COLOR_YELLOW},
                                                   {"ERR", ANSI_COLOR_RED},
                                                   {"CRT", ANSI_COLOR_RED}};
    const auto level_clamped = std::min(level, Level::_max);
    const auto level_descr_idx = level_clamped == 0 ? 0 : ((level - 1) / 10);
    std::string_view thread_name = "?";
    if (ln::interrupt_context()) {
        thread_name = "ISR!";
    }
    else {
        const auto _thread_name = ln::get_thread_name();
        if (!_thread_name.empty()) {
            thread_name = _thread_name;
        }
    }
    using namespace std::string_view_literals;
    return this->print_buf(
        "{}|{}|{}|{}{}{}|", Clock::now(), thread_name, module.name,
        (this->config.color ? level_descrs[level_descr_idx].color : ""sv),
        level_descrs[level_descr_idx].tag_name,
        (this->config.color ? ANSI_COLOR_DEFAULT : ""sv));
}

void Logger::_flush_buffer() {
    if (this->out_buf_len == 0) {
        return;
    }
    std::fwrite(this->config.out_buf.data(), sizeof(char), this->out_buf_len,
                this->config.out_file.c_file());
    this->out_buf_len = 0;
}

} // namespace ln::logger
