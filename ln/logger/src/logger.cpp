/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/logger/logger.hpp"
#include "ln/ln.hpp"

#include <FreeRTOS/Addons/LockGuard.hpp>
#include <FreeRTOS/Addons/Clock.hpp>

#include <cstdio>

namespace ln::logger {

extern "C" void ln_logger_log(LoggerModule *module, LoggerLevel level, const char *fmt, ...) {
    if (!module) {
        return;
    }
    if (!Logger::get_instance().is_enabled()) {
        return;
    }
    if (level < (module->log_level == LOGGER_LEVEL_NOTSET ? Logger::get_instance().get_config().log_level
                                                          : module->log_level)) {
        return;
    }
    va_list arg_list;
    va_start(arg_list, fmt);
    Logger::get_instance().log(*module, level, fmt, arg_list);
    va_end(arg_list);
}

void Logger::enable() { get_instance().config.enabled_run_time = true; }

extern "C" void ln_logger_enable() { Logger::get_instance().enable(); }

Logger &Logger::get_instance() {
    static Logger instance;
    return instance;
};

bool Logger::is_enabled() { return Config::enabled_compile_time && get_instance().config.enabled_run_time; }

extern "C" void ln_logger_flush_buffer() { Logger::get_instance().flush_buffer(); }

void Logger::flush_buffer() {
    if (ln::is_inside_interrupt()) {
        LN_PANIC();
    }
    FreeRTOS::Addons::LockGuard lock_guard(this->mutex);
    if (!this->is_enabled()) {
        return;
    }
    this->flush_buffer_unsafe();
}

void Logger::clear_buffer_unsafe() {
    ln::File file(this->buff_mem, "w"); // effectively clears the buffer
}

void Logger::flush_buffer_unsafe() {
    std::fwrite(this->buff_mem.data(), 1, std::min(strlen(this->buff_mem.data()), this->buff_mem.size()),
                this->config.out_file);
    this->clear_buffer_unsafe();
}

void Logger::set_level(Level log_level) { this->config.log_level = log_level; }

bool Logger::set_config(const Config &config) {
    // TODO: validation here
    this->config = config;
    return true;
}

Module::Module(const std::string_view name, Level log_level) {
    this->name = name.data();
    this->log_level = log_level;
}

void Module::log(const Level &level, const std::string_view fmt, ...) {
    if (!Logger::get_instance().is_enabled()) {
        return;
    }
    if (level < (this->log_level == LOGGER_LEVEL_NOTSET ? Logger::get_instance().config.log_level : this->log_level)) {
        return;
    }
    va_list arg_list;
    va_start(arg_list, fmt);
    Logger::get_instance().log(*this, level, fmt, arg_list);
    va_end(arg_list);
}

void Module::set_level(Level log_level) { this->log_level = log_level; }

int Logger::log(const LoggerModule &module, const Logger::Level &level, const std::string_view fmt,
                const va_list &arg_list) {
    const auto is_interrupt_context = ln::is_inside_interrupt();
    if (!is_interrupt_context && !this->mutex.lock()) {
        return 0;
    }
    const auto rc = this->log_unsafe(module, level, fmt, arg_list);
    if (!is_interrupt_context) {
        if (strlen(this->buff_mem.data()) > Config::out_buffer_auto_flush_threshold) {
            this->flush_buffer_unsafe();
        }
        this->mutex.unlock();
    }
    return rc;
}
int Logger::log_unsafe(const LoggerModule &module, const Logger::Level &level, const std::string_view fmt,
                       const va_list &arg_list) {
    ln::File buff_file(this->buff_mem, "a+");
    int chars_printed = 0;
    if (this->config.print_header_enabled) {
        LN_CHECK(this->print_header(buff_file, module, level), rc, rc < 0, { chars_printed += rc; }, {});
    }
    LN_CHECK(vfprintf(buff_file, fmt.data(), arg_list), rc, rc < 0, { chars_printed += rc; }, {});
    LN_CHECK(fprintf(buff_file, "%s", this->config.eol), rc, rc < 0, { chars_printed += rc; }, {});
    return chars_printed;
}

int Logger::print_header(ln::File &file, const LoggerModule &module, const Logger::Level &level) {

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
    const auto level_clamped = std::min(level, LOGGER_LEVEL_MAX_);
    const auto level_descr_idx = level_clamped == 0 ? 0 : ((level - 1) / 10);
    using Clock = FreeRTOS::Addons::Clock;
    const auto [tm_buf, sec_remainder] = Clock::to_utc_tm_rem(Clock::now());
    char datetime_buffer[20];
    const auto ms =
        static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::milliseconds>(sec_remainder).count());
    std::strftime(datetime_buffer, sizeof(datetime_buffer), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return this->printf(file, "%s.%03lu|%s%s%s|%s%s|%s|", datetime_buffer, ms,
                        (this->config.color ? level_descrs[level_descr_idx].color.data() : ""),
                        level_descrs[level_descr_idx].tag_name.data(), (this->config.color ? ANSI_COLOR_DEFAULT : ""),
                        (ln::is_inside_interrupt() ? "ISR!" : ""), get_current_thread_name(), module.name);
}

int Logger::printf(ln::File &file, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const auto rc = vfprintf(file, fmt, args);
    va_end(args);
    return rc;
}

} // namespace ln::logger
