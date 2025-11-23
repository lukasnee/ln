add_library(ln_shell_cmds_core INTERFACE)
add_library(ln::shell::cmds::core ALIAS ln_shell_cmds_core)
target_link_libraries(ln_shell_cmds_core INTERFACE ln::shell)
target_sources(ln_shell_cmds_core INTERFACE clear.cpp echo.cpp hexdump.cpp
                                            repeat.cpp)
if(LN_LUA)
  target_link_libraries(ln_shell_cmds_core INTERFACE lua)
  target_sources(ln_shell_cmds_core INTERFACE lua.cpp)
endif()
