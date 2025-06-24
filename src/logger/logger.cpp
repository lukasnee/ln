#include "fonas/logger/logger.hpp"
#include "fonas/fonas.hpp"

#include <cstdio>

namespace fonas {

extern "C" void fonas_logger_log(LoggerModule *module, LoggerLevel level, const char *fmt, ...) {
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

extern "C" void fonas_logger_enable() { Logger::get_instance().enable(); }

Logger &Logger::get_instance() {
    static Logger instance;
    return instance;
};

bool Logger::is_enabled() { return Logger::Config::enabled_compile_time && get_instance().config.enabled_run_time; }

extern "C" void fonas_logger_flush_buffer() { Logger::get_instance().flush_buffer(); }

void Logger::flush_buffer() {
    if (fonas::is_inside_interrupt()) {
        FONAS_PANIC();
    }
    LockGuard lock_guard(this->mutex);
    if (!this->is_enabled()) {
        return;
    }
    this->flush_buffer_unsafe();
}

void Logger::flush_buffer_unsafe() {
    std::fwrite(this->buff_mem.data(), 1, std::min(strlen(this->buff_mem.data()), this->buff_mem.size()),
                this->config.out_file);
    {
        fonas::File file(this->buff_mem.data(), this->buff_mem.size(), "w"); // effectively clears the buffer
    }
}

void Logger::set_level(Level log_level) { this->config.log_level = log_level; }

Logger::Module::Module(const std::string_view name, Level log_level) {
    this->name = name.data();
    this->log_level = log_level;
}

void Logger::Module::log(const Level &level, const std::string_view fmt, ...) {
    if (!Logger::get_instance().is_enabled()) {
        return;
    }
    if (level < (this->log_level == LOGGER_LEVEL_NOTSET ? get_instance().config.log_level : this->log_level)) {
        return;
    }
    va_list arg_list;
    va_start(arg_list, fmt);
    get_instance().log(*this, level, fmt, arg_list);
    va_end(arg_list);
}

void Logger::Module::set_level(Level log_level) { this->log_level = log_level; }

int Logger::log(const LoggerModule &module, const Logger::Level &level, const std::string_view fmt,
                const va_list &arg_list) {
    const auto is_interrupt_context = fonas::is_inside_interrupt();
    if (!is_interrupt_context && !this->mutex.Lock()) {
        return 0;
    }
    const auto rc = this->log_unsafe(module, level, fmt, arg_list);
    if (!is_interrupt_context) {
        if (strlen(this->buff_mem.data()) > Config::out_buffer_auto_flush_threshold) {
            this->flush_buffer_unsafe();
        }
        this->mutex.Unlock();
    }
    return rc;
}
int Logger::log_unsafe(const LoggerModule &module, const Logger::Level &level, const std::string_view fmt,
                       const va_list &arg_list) {
    fonas::File buff_file(this->buff_mem.data(), this->buff_mem.size(), "a+");
    int chars_printed = 0;
    if (this->config.print_header_enabled) {
        FONAS_CHECK(this->print_header(buff_file, module, level), rc, rc < 0, { chars_printed += rc; }, {});
    }
    FONAS_CHECK(vfprintf(buff_file, fmt.data(), arg_list), rc, rc < 0, { chars_printed += rc; }, {});
    FONAS_CHECK(fputc('\n', buff_file), rc, rc < 0, { chars_printed += rc; }, {});
    return chars_printed;
}

int Logger::print_header(fonas::File &file, const LoggerModule &module, const Logger::Level &level) {

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
    char datetime_buffer[20];
    const auto timestamp = fonas::get_timestamp();
    std::strftime(datetime_buffer, sizeof(datetime_buffer), "%Y-%m-%d %H:%M:%S", &timestamp.tm);
    return this->printf(file, "%s.%03lu|%s%s%s|%s%s|%s|", datetime_buffer, timestamp.ms,
                        (this->config.color ? level_descrs[level_descr_idx].color.data() : ""),
                        level_descrs[level_descr_idx].tag_name.data(), (this->config.color ? ANSI_COLOR_DEFAULT : ""),
                        (fonas::is_inside_interrupt() ? "ISR!" : ""), get_current_thread_name(), module.name);
}

int Logger::printf(fonas::File &file, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const auto rc = vfprintf(file, fmt, args);
    va_end(args);
    return rc;
}

} // namespace fonas
