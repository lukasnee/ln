set(CACHE{LN_SHELL_CMD_EXCLUDE} STRING "List of commands to exclude" VALUE "")

ln_add_library(ln_shell_cmds_general ALIAS ln::shell::cmds::general INTERFACE)
target_link_libraries(ln_shell_cmds_general INTERFACE ln::shell)

set(LN_SHELL_CMD_GENERAL_SOURCES clear.cpp echo.cpp hexdump.cpp repeat.cpp
                                 sleep.cpp)
if(NOT LN_SHELL_CMD_EXCLUDE STREQUAL "")
  string(REPLACE "," ";" LN_SHELL_CMD_EXCLUDE_LIST "${LN_SHELL_CMD_EXCLUDE}")
  foreach(excluded_cmd ${LN_SHELL_CMD_EXCLUDE_LIST})
    list(FILTER LN_SHELL_CMD_GENERAL_SOURCES EXCLUDE REGEX
         "^${excluded_cmd}\\.cpp$")
  endforeach()
endif()
message(STATUS "ln_shell_cmds_general sources: ${LN_SHELL_CMD_GENERAL_SOURCES}")
target_sources(ln_shell_cmds_general INTERFACE ${LN_SHELL_CMD_GENERAL_SOURCES})
