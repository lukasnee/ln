#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum {
        LOGGER_LEVEL_NOTSET = 0,
        LOGGER_LEVEL_LOWEST = 1,
        LOGGER_LEVEL_DEBUG3 = 8,
        LOGGER_LEVEL_DEBUG2 = 9,
        LOGGER_LEVEL_DEBUG = 10,
        LOGGER_LEVEL_INFO = 20,
        LOGGER_LEVEL_WARNING = 30,
        LOGGER_LEVEL_ERROR = 40,
        LOGGER_LEVEL_CRITICAL = 50,

        LOGGER_LEVEL_MAX_ = LOGGER_LEVEL_CRITICAL,
    } LoggerLevel;

    typedef struct {
        const char *name;
        LoggerLevel log_level;
    } LoggerModule;

    void fonas_logger_enable();

    void fonas_logger_log(LoggerModule *module, LoggerLevel level, const char *fmt, ...);

    void fonas_logger_flush_buffer();

#define LOG_SCOPE(_logger_module) LoggerModule *__logger_curr_scope = &_logger_module;

#define LOG_MODULE_DEFINITION(_obj_name, _name, _level) LoggerModule _obj_name = {.name = #_name, .log_level = _level};

#define LOG_MODULE_EXT(_obj_name)                                                                                      \
    extern LoggerModule _obj_name;                                                                                     \
    LOG_SCOPE(_obj_name)

#define LOG_MODULE(_name, _level)                                                                                      \
    LOG_MODULE_DEFINITION(logger_module, _name, _level);                                                               \
    LOG_SCOPE(logger_module)

#define LOG(_level, ...) fonas_logger_log(__logger_curr_scope, _level, __VA_ARGS__);
#define LOG_DEBUG(...) LOG(LOGGER_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) LOG(LOGGER_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARNING(...) LOG(LOGGER_LEVEL_WARNING, __VA_ARGS__)
#define LOG_ERROR(...) LOG(LOGGER_LEVEL_ERROR, __VA_ARGS__)
#define LOG_CRITICAL(...) LOG(LOGGER_LEVEL_CRITICAL, __VA_ARGS__)

#define LOG_FLUSH() fonas_logger_flush_buffer()

#ifdef __cplusplus
}
#endif
