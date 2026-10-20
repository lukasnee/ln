ln_add_library(ln_logger ALIAS ln::logger)
target_sources(ln_logger PRIVATE src/logger.cpp)
target_compile_definitions(ln_logger PUBLIC LN_LOGGER)
target_include_directories(ln_logger PUBLIC include)
target_link_libraries(ln_logger PUBLIC ln::core)

ln_add_library(ln_logger_cmds ALIAS ln::logger::cmds INTERFACE)
target_sources(ln_logger_cmds INTERFACE src/cmds.cpp)
target_link_libraries(ln_logger_cmds INTERFACE ln::logger ln::shell)

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
  add_subdirectory(tests)
endif()
