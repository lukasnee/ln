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
    cpp_freertos::LockGuard lock(get_instance().mutex);
    int chars_printed = 0;
    if (get_instance().config.header) {
        int res = get_instance().print_header(module, level);
        if (res < 0) {
            return res;
        }
        chars_printed += res;
    }
    {
        int res = vprintf(fmt.data(), arg_list);
        if (res < 0) {
            return res;
        }
        chars_printed += res;
    }
    {
        int res = putchar('\n');
        if (res < 0) {
            return res;
        }
        chars_printed += 1;
    }
    return chars_printed;
}

int Logger::print_header(const LoggerModule &module, const Logger::Level &level) {

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
    return printf("%s.%03lu|%s%s%s|%s|%s|", datetime_buffer, timestamp.ms,
                  (this->config.color ? level_descrs[level_descr_idx].color.data() : ""),
                  level_descrs[level_descr_idx].tag_name.data(), (this->config.color ? ANSI_COLOR_DEFAULT : ""),
                  get_current_thread_name(), module.name);
}

} // namespace fonas
