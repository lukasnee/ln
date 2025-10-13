add_library(ln_shell_cmds_core)
add_library(ln::shell::cmds::core ALIAS ln_shell_cmds_core)
target_link_libraries(ln_shell_cmds_core PUBLIC ln::shell)
target_sources(ln_shell_cmds_core PRIVATE clear.cpp echo.cpp hexdump.cpp
                                          repeat.cpp)
