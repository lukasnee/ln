// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

// Prefer including "ln/log.h" for the macro API, it is a no-op when the logger
// is not built in.

// Can be used together with LN_LOG_SCOPE() to use already defined module from
// another source file. LN_LOG_EXTERN() must be used in global scope, even
// though while LN_LOG_SCOPE() is scope-based. If external module is in a
// different namespace, wrap the LN_LOG_EXTERN(<name>) call in `namespace
// <some_namespace>
// {}` and use qualified name in LN_LOG_SCOPE(<some_namespace>::<name>).
#define LN_LOG_EXTERN(_name) extern ::ln::logger::Module _name##_logger_module

#define LN_LOG_SCOPE(_name) _LN_LOG_SCOPE(::_name)

#define _LN_LOG_SCOPE(_name)                                                   \
    ::ln::logger::Module *__logger_module_curr_scope [[maybe_unused]] =        \
        &_name##_logger_module

#define LN_LOG_MODULE_DEFINITION(_name, _level)                                \
    ::ln::logger::Module _name##_logger_module {                               \
        #_name, _level, ::ln::logger::get_instance()                           \
    }

#define LN_LOG_MODULE(_name, _level)                                           \
    LN_LOG_MODULE_DEFINITION(_name, _level);                                   \
    static _LN_LOG_SCOPE(_name)

#define LN_LOG_MODULE_CLASS_MEMBER(_name, _level)                              \
    LN_LOG_MODULE_DEFINITION(_name, _level);                                   \
    _LN_LOG_SCOPE(_name)

#define LN_LOG(_level, ...)                                                    \
    __logger_module_curr_scope->log(_level, __VA_ARGS__);
#define LN_LOG_DBG(...) LN_LOG(::ln::logger::Level::debug, __VA_ARGS__)
#define LN_LOG_INF(...) LN_LOG(::ln::logger::Level::info, __VA_ARGS__)
#define LN_LOG_WRN(...) LN_LOG(::ln::logger::Level::warning, __VA_ARGS__)
#define LN_LOG_ERR(...) LN_LOG(::ln::logger::Level::error, __VA_ARGS__)
#define LN_LOG_CRT(...) LN_LOG(::ln::logger::Level::critical, __VA_ARGS__)

#define LN_LOG_FLUSH() ::ln::logger::get_instance().flush_buffer()

#include "ln/File.hpp"
#include "ln/Mutex.hpp"

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <span>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace ln::logger {

enum Level {
    notset = 0,
    lowest = 1,
    debug = 10,
    info = 20,
    warning = 30,
    error = 40,
    critical = 50,

    _max = critical,
};

struct Config {
    /* Output stream */
    File out_file = File(stdout);
    /* Output buffer for formatting log messages before flushing to the output
    stream. If empty, log messages are formatted directly to the output stream
    without buffering. */
    std::span<char> out_buf = {};
    /* Output buffer flush threshold (effective only if out_buf is not empty).
    If free space is less than this value after a log message is written to the
    buffer, the content will be flushed to the out_file. You probably want this
    size threshold to be of an average log message or a bit more (there's a
    tradeoff between buffer use efficiency and risk of overflow and loss of part
    of the message) */
    static constexpr size_t out_buf_flush_threshold = 128;
    /* Switch logger on/off at compile time */
    static constexpr bool enabled_compile_time = true;
    /* Switch logger on/off at run-time */
    bool enabled_run_time = false;
    /* Global log level printing threshold */
    Level log_level = Level::info;
    /* Colorize log messages */
    bool color = false;
    /* end of line character(s) */
    const char *eol = "\n";
    /* Print log message header */
    bool print_header_enabled = true;
};

class Module;

/**
 * @brief RTOS logger with Python logging style.
 */
class Logger {
public:
    static Logger &get_instance();

    void enable();

    void set_level(Level log_level);

    bool set_config(const Config &config);

    const Config &get_config() const { return config; }

    template <typename... Args>
    int log(const Module &module, const Level &level, fmt::string_view fmt_str,
            Args &&...args) {
        const auto is_interrupt_context = ln::interrupt_context();
        if (!is_interrupt_context && !this->mutex.lock()) {
            return 0;
        }
        const auto rc =
            this->_log(module, level, fmt_str, std::forward<Args>(args)...);
        if (!is_interrupt_context) {
            this->mutex.unlock();
        }
        return rc;
    }

    /**
     * @brief Flush the output buffer to the output stream. Note that buffer
     * is flushed automatically when it reaches the
     * Config::out_buffer_auto_flush_threshold size.
     *
     * This function is thread-safe and cannot be called from an ISR
     * context.
     */
    void flush_buffer();

protected:
    Config config = {};

    friend class Module;

private:
    Logger() = default;
    Logger(Logger const &) = delete;
    void operator=(Logger const &) = delete;
    ~Logger() = default;

    template <typename... Args>
    int _log(const Module &module, const Level &level, fmt::string_view fmt_str,
             Args &&...args) {
        int chars_printed = 0;
        if (this->config.print_header_enabled) {
            LN_CHECK(this->print_header(module, level), rc, rc < 0,
                     { chars_printed += rc; }, {});
        }
        LN_CHECK(this->print_buf(fmt_str, std::forward<Args>(args)...), rc,
                 rc < 0, { chars_printed += rc; }, {});
        LN_CHECK(this->print_buf("{}", this->config.eol), rc, rc < 0,
                 { chars_printed += rc; }, {});
        return chars_printed;
    }

    void _flush_buffer();

    int print_header(const Module &module, const Level &level);

    template <typename... Args>
    int print_buf(fmt::string_view fmt_str, Args &&...args) {
        if (!this->config.out_buf.empty()) {
            auto out = this->config.out_buf.data() + this->out_buf_len;
            auto cap = this->config.out_buf.size() - this->out_buf_len;
            auto res = fmt::vformat_to_n(out, cap, fmt_str,
                                         fmt::make_format_args(args...));
            if (res.size <= 0) {
                return static_cast<int>(res.size);
            }
            this->out_buf_len += static_cast<size_t>(res.size);
            if (this->config.out_buf.size() - this->out_buf_len <
                Config::out_buf_flush_threshold) {
                this->_flush_buffer();
            }
            return static_cast<int>(res.size);
        }
        fmt::vprint(this->config.out_file.c_file(), fmt_str,
                    fmt::make_format_args(args...));
        return 0;
    }

    RecursiveMutex mutex;

    size_t out_buf_len = 0;
};

/**
 * @brief A shorthand for logger::Logger::get_instance().
 *
 * @retval Logger&
 */
Logger &get_instance();

class Module {
public:
    explicit Module(std::string_view name, Level log_level = Level::notset,
                    Logger &logger = Logger::get_instance());

    template <typename... Args> void debug(const char *fmt, Args &&...args) {
        this->log(Level::debug, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args> void info(const char *fmt, Args &&...args) {
        this->log(Level::info, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args> void warning(const char *fmt, Args &&...args) {
        this->log(Level::warning, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args> void error(const char *fmt, Args &&...args) {
        this->log(Level::error, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args> void critical(const char *fmt, Args &&...args) {
        this->log(Level::critical, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void log(const Level &level, fmt::string_view fmt_str, Args &&...args) {
        if constexpr (!Config::enabled_compile_time) {
            return;
        }
        if (!this->logger.config.enabled_run_time) {
            return;
        }
        if (level < (this->log_level == Level::notset
                         ? this->logger.config.log_level
                         : this->log_level)) {
            return;
        }
        this->logger.log(*this, level, fmt_str, std::forward<Args>(args)...);
    }

    void set_level(Level log_level);

private:
    friend class Logger;

    std::string_view name;
    Level log_level;
    Logger &logger;
};

} // namespace ln::logger
