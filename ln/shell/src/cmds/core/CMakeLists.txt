add_library(ln_shell_cmds_general INTERFACE)
add_library(ln::shell::cmds::general ALIAS ln_shell_cmds_general)
target_link_libraries(ln_shell_cmds_general INTERFACE ln::shell)
target_sources(ln_shell_cmds_general INTERFACE clear.cpp echo.cpp hexdump.cpp
                                               repeat.cpp)
if(LN_LUA)
  target_link_libraries(ln_shell_cmds_general INTERFACE lua)
  target_sources(ln_shell_cmds_general INTERFACE lua.cpp)
endif()
