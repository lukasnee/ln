ln_add_library(ln_core ALIAS ln::core)
target_link_libraries(ln_core PUBLIC ln::port::${LN_PORT} fmt::fmt ln::logger)
target_sources(ln_core PRIVATE src/ln.cpp)
target_include_directories(ln_core PUBLIC include)

ln_add_library(ln_core_shell_cmds ALIAS ln::core::shell_cmds INTERFACE)
target_link_libraries(ln_core_shell_cmds INTERFACE ln::core ln::shell)
target_sources(ln_core_shell_cmds INTERFACE src/ln_shell_cmds.cpp)

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
  add_subdirectory(tests)
endif()
